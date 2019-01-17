
static void set_params(void)
{
    snd_pcm_hw_params_t *params;
    snd_pcm_sw_params_t *swparams;
    snd_pcm_uframes_t buffer_size;
    int err;
    size_t n;
    unsigned int rate;
    snd_pcm_uframes_t start_threshold, stop_threshold;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_sw_params_alloca(&swparams);
    err = snd_pcm_hw_params_any(handle, params);
    if (err < 0) {
        error(_("Broken configuration for this PCM: no configurations available"));
        //exit(EXIT_FAILURE);
       }
    else if (interleaved)
        err = snd_pcm_hw_params_set_access(handle, params,
                                           SND_PCM_ACCESS_RW_INTERLEAVED);
    else
        err = snd_pcm_hw_params_set_access(handle, params,
                                           SND_PCM_ACCESS_RW_NONINTERLEAVED);
    if (err < 0) {
        error(_("Access type not available"));
       // exit(EXIT_FAILURE);
       }
    err = snd_pcm_hw_params_set_format(handle, params, hwparams.format);
    if (err < 0) {
        error(_("Sample format non available"));
        //exit(EXIT_FAILURE);
    }
    err = snd_pcm_hw_params_set_channels(handle, params, hwparams.channels);
    if (err < 0) {
        error(_("Channels count non available"));
        //exit(EXIT_FAILURE);
    }

#if 0
    err = snd_pcm_hw_params_set_periods_min(handle, params, 2);
    assert(err >= 0);
#endif
    rate = hwparams.rate;
    err = snd_pcm_hw_params_set_rate_near(handle, params, &hwparams.rate, 0);
    assert(err >= 0);
    if ((float)rate * 1.05 < hwparams.rate || (float)rate * 0.95 > hwparams.rate) {
        if (!quiet_mode) {
            char plugex[64];
            const char *pcmname = snd_pcm_name(handle);
            fprintf(stderr, _("Warning: rate is not accurate (requested = %iHz, got = %iHz)\n"), rate, hwparams.rate);
            if (! pcmname || strchr(snd_pcm_name(handle), ':'))
                *plugex = 0;
            else
                snprintf(plugex, sizeof(plugex), "(-Dplug:%s)",
                         snd_pcm_name(handle));
            fprintf(stderr, _("         please, try the plug plugin %s\n"),
                    plugex);
        }
    }
    rate = hwparams.rate;
    if (buffer_time == 0 && buffer_frames == 0) {
        err = snd_pcm_hw_params_get_buffer_time_max(params,
                                                    &buffer_time, 0);
        assert(err >= 0);
        if (buffer_time > 500000)
            buffer_time = 500000;
    }
    if (period_time == 0 && period_frames == 0) {
        if (buffer_time > 0)
            period_time = buffer_time / 4;
        else
            period_frames = buffer_frames / 4;
    }
    if (period_time > 0)
        err = snd_pcm_hw_params_set_period_time_near(handle, params,
                                                     &period_time, 0);
    else
        err = snd_pcm_hw_params_set_period_size_near(handle, params,
                                                     &period_frames, 0);
    assert(err >= 0);
    if (buffer_time > 0) {
        err = snd_pcm_hw_params_set_buffer_time_near(handle, params,
                                                     &buffer_time, 0);
    } else {
        err = snd_pcm_hw_params_set_buffer_size_near(handle, params,
                                                     &buffer_frames);
    }
    assert(err >= 0);
    err = snd_pcm_hw_params(handle, params);
    if (err < 0) {
        error(_("Unable to install hw params:"));
        //snd_pcm_hw_params_dump(params, log);
        //exit(EXIT_FAILURE);
    }
    snd_pcm_hw_params_get_period_size(params, &chunk_size, 0);
    snd_pcm_hw_params_get_buffer_size(params, &buffer_size);
    if (chunk_size == buffer_size) {
        error(_("Can't use period equal to buffer size (%lu == %lu)"),
              chunk_size, buffer_size);
        //exit(EXIT_FAILURE);
    }
    snd_pcm_sw_params_current(handle, swparams);
    if (avail_min < 0)
        n = chunk_size;
    else
        n = (double) rate * avail_min / 1000000;
    err = snd_pcm_sw_params_set_avail_min(handle, swparams, n);

    /* round up to closest transfer boundary */
    n = buffer_size;
    if (start_delay <= 0) {
        start_threshold = n + (double) rate * start_delay / 1000000;
    } else
        start_threshold = (double) rate * start_delay / 1000000;
    if (start_threshold < 1)
        start_threshold = 1;
    if (start_threshold > n)
        start_threshold = n;
    err = snd_pcm_sw_params_set_start_threshold(handle, swparams, start_threshold);
    assert(err >= 0);
    if (stop_delay <= 0)
        stop_threshold = buffer_size + (double) rate * stop_delay / 1000000;
    else
        stop_threshold = (double) rate * stop_delay / 1000000;
    err = snd_pcm_sw_params_set_stop_threshold(handle, swparams, stop_threshold);
    assert(err >= 0);

    if (snd_pcm_sw_params(handle, swparams) < 0) {
        error(_("unable to install sw params:"));
        //snd_pcm_sw_params_dump(swparams, log);
        //exit(EXIT_FAILURE);
    }

    if (verbose)
        //snd_pcm_dump(handle, log);

        bits_per_sample = snd_pcm_format_physical_width(hwparams.format);
    bits_per_sample = 16;
    bits_per_frame = bits_per_sample * hwparams.channels;
    chunk_bytes = chunk_size * bits_per_frame / 8;
    audiobuf = (u_char *)realloc(audiobuf, chunk_bytes);
    if (audiobuf == NULL) {
        error(_("not enough memory"));
        //exit(EXIT_FAILURE);
    }
    // fprintf(stderr, "real chunk_size = %i, frags = %i, total = %i\n", chunk_size, setup.buf.block.frags, setup.buf.block.frags * chunk_size);

    /* stereo VU-meter isn't always available... */
    if (vumeter == VUMETER_STEREO) {
        if (hwparams.channels != 2 || !interleaved || verbose > 2)
            vumeter = VUMETER_MONO;
    }

    buffer_frames = buffer_size;	/* for position test */
}




void audiothread::run()
{
    QElapsedTimer audio_render_timer;
    audio_render_timer.start();

    set_params();
    for(;;)
    {
        if(g_b_start == false)
        {
            qDebug()<<" exit from audio thread";
            break;
        }
        capture();
    }
    qDebug() << "Total Audio time : " << audio_render_timer.elapsed() << "milliseconds";
}



#if 0
static void xrun(void)
{
    snd_pcm_status_t *status;
    int res;

    snd_pcm_status_alloca(&status);
    if ((res = snd_pcm_status(handle, status))<0) {
        error(_("status error: %s"), snd_strerror(res));
        exit(EXIT_FAILURE);
    }
    if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
        struct timeval now, diff, tstamp;
        gettimeofday(&now, 0);
        snd_pcm_status_get_trigger_tstamp(status, &tstamp);
        timersub(&now, &tstamp, &diff);
        fprintf(stderr, _("%s!!! (at least %.3f ms long)\n"),
                stream == SND_PCM_STREAM_PLAYBACK ? _("underrun") : _("overrun"),
                diff.tv_sec * 1000 + diff.tv_usec / 1000.0);
        if (verbose) {
            fprintf(stderr, _("Status:\n"));
            //snd_pcm_status_dump(status, log);
        }
        if ((res = snd_pcm_prepare(handle))<0) {
            error(_("xrun: prepare error: %s"), snd_strerror(res));
            exit(EXIT_FAILURE);
        }
        return;		/* ok, data should be accepted again */
    } if (snd_pcm_status_get_state(status) == SND_PCM_STATE_DRAINING) {
        if (verbose) {
            fprintf(stderr, _("Status(DRAINING):\n"));
            //snd_pcm_status_dump(status, log);
        }
        if (stream == SND_PCM_STREAM_CAPTURE) {
            fprintf(stderr, _("capture stream format change? attempting recover...\n"));
            if ((res = snd_pcm_prepare(handle))<0) {
                error(_("xrun(DRAINING): prepare error: %s"), snd_strerror(res));
                exit(EXIT_FAILURE);
            }
            return;
        }
    }
    if (verbose) {
        fprintf(stderr, _("Status(R/W):\n"));
        //snd_pcm_status_dump(status, log);
    }
    error(_("read/write error, state = %s"), snd_pcm_state_name(snd_pcm_status_get_state(status)));
    exit(EXIT_FAILURE);
} //xrun

/* I/O suspend handler */
static void suspend(void)
{
    int res;

    if (!quiet_mode)
        fprintf(stderr, _("Suspended. Trying resume. ")); fflush(stderr);
    while ((res = snd_pcm_resume(handle)) == -EAGAIN)
        sleep(1);	/* wait until suspend flag is released */
    if (res < 0) {
        if (!quiet_mode)
            fprintf(stderr, _("Failed. Restarting stream. ")); fflush(stderr);
        if ((res = snd_pcm_prepare(handle)) < 0) {
            error(_("suspend: prepare error: %s"), snd_strerror(res));
            exit(EXIT_FAILURE);
        }
    }
    if (!quiet_mode)
        fprintf(stderr, _("Done.\n"));
} //suspend

static void print_vu_meter_mono(int perc, int maxperc)
{
    const int bar_length = 50;
    char line[80];
    int val;

    for (val = 0; val <= perc * bar_length / 100 && val < bar_length; val++)
        line[val] = '#';
    for (; val <= maxperc * bar_length / 100 && val < bar_length; val++)
        line[val] = ' ';
    line[val] = '+';
    for (++val; val <= bar_length; val++)
        line[val] = ' ';
    if (maxperc > 99)
        sprintf(line + val, "| MAX");
    else
        sprintf(line + val, "| %02i%%", maxperc);
    fputs(line, stdout);
    if (perc > 100)
        printf(_(" !clip  "));
} //print_vu_meter_mono

static void print_vu_meter_stereo(int *perc, int *maxperc)
{
    const int bar_length = 35;
    char line[80];
    int c;

    memset(line, ' ', sizeof(line) - 1);
    line[bar_length + 3] = '|';

    for (c = 0; c < 2; c++) {
        int p = perc[c] * bar_length / 100;
        char tmp[4];
        if (p > bar_length)
            p = bar_length;
        if (c)
            memset(line + bar_length + 6 + 1, '#', p);
        else
            memset(line + bar_length - p - 1, '#', p);
        p = maxperc[c] * bar_length / 100;
        if (p > bar_length)
            p = bar_length;
        if (c)
            line[bar_length + 6 + 1 + p] = '+';
        else
            line[bar_length - p - 1] = '+';
        if (maxperc[c] > 99)
            sprintf(tmp, "MAX");
        else
            sprintf(tmp, "%02d%%", maxperc[c]);
        if (c)
            memcpy(line + bar_length + 3 + 1, tmp, 3);
        else
            memcpy(line + bar_length, tmp, 3);
    }
    line[bar_length * 2 + 6 + 2] = 0;
    fputs(line, stdout);
}//print_vu_meter_stereo
static void print_vu_meter(signed int *perc, signed int *maxperc)
{
    if (vumeter == VUMETER_STEREO)
        print_vu_meter_stereo(perc, maxperc);
    else
        print_vu_meter_mono(*perc, *maxperc);
}//print_vu_meter

/* peak handler */
static void compute_max_peak(u_char *data, size_t count)
{
    signed int val, max, perc[2], max_peak[2];
    static	int	run = 0;
    //size_t ocount = count;
    int	format_little_endian = snd_pcm_format_little_endian(hwparams.format);
    int ichans, c;

    if (vumeter == VUMETER_STEREO)
        ichans = 2;
    else
        ichans = 1;

    memset(max_peak, 0, sizeof(max_peak));
    switch (bits_per_sample) {
    case 8: {
        signed char *valp = (signed char *)data;
        signed char mask = snd_pcm_format_silence(hwparams.format);
        c = 0;
        while (count-- > 0) {
            val = *valp++ ^ mask;
            val = abs(val);
            if (max_peak[c] < val)
                max_peak[c] = val;
            if (vumeter == VUMETER_STEREO)
                c = !c;
        }
        break;
    }
    case 16: {
        signed short *valp = (signed short *)data;
        signed short mask = snd_pcm_format_silence_16(hwparams.format);
        signed short sval;

        count /= 2;
        c = 0;
        while (count-- > 0) {
            if (format_little_endian)
                sval = __le16_to_cpu(*valp);
            else
                sval = __be16_to_cpu(*valp);
            sval = abs(sval) ^ mask;
            if (max_peak[c] < sval)
                max_peak[c] = sval;
            valp++;
            if (vumeter == VUMETER_STEREO)
                c = !c;
        }
        break;
    }
    case 24: {
        unsigned char *valp = data;
        signed int mask = snd_pcm_format_silence_32(hwparams.format);

        count /= 3;
        c = 0;
        while (count-- > 0) {
            if (format_little_endian) {
                val = valp[0] | (valp[1]<<8) | (valp[2]<<16);
            } else {
                val = (valp[0]<<16) | (valp[1]<<8) | valp[2];
            }
            /* Correct signed bit in 32-bit value */
            if (val & (1<<(bits_per_sample-1))) {
                val |= 0xff<<24;	/* Negate upper bits too */
            }
            val = abs(val) ^ mask;
            if (max_peak[c] < val)
                max_peak[c] = val;
            valp += 3;
            if (vumeter == VUMETER_STEREO)
                c = !c;
        }
        break;
    }
    case 32: {
        signed int *valp = (signed int *)data;
        signed int mask = snd_pcm_format_silence_32(hwparams.format);

        count /= 4;
        c = 0;
        while (count-- > 0) {
            if (format_little_endian)
                val = __le32_to_cpu(*valp);
            else
                val = __be32_to_cpu(*valp);
            val = abs(val) ^ mask;
            if (max_peak[c] < val)
                max_peak[c] = val;
            valp++;
            if (vumeter == VUMETER_STEREO)
                c = !c;
        }
        break;
    }
    default:
        if (run == 0) {
            // fprintf(stderr, _("Unsupported bit size %d.\n"), (int)bits_per_sample);
            qDebug() << "Unsupported bit size : " << (int)bits_per_sample;
            run = 1;
        }
        return;
    }
    max = 1 << (bits_per_sample-1);
    if (max <= 0)
        max = 0x7fffffff;

    for (c = 0; c < ichans; c++) {
        if (bits_per_sample > 16)
            perc[c] = max_peak[c] / (max / 100);
        else
            perc[c] = max_peak[c] * 100 / max;
    }

    if (interleaved && verbose <= 2) {
        static int maxperc[2];
        static time_t t=0;
        const time_t tt=time(NULL);
        if(tt>t) {
            t=tt;
            maxperc[0] = 0;
            maxperc[1] = 0;
        }
        for (c = 0; c < ichans; c++)
            if (perc[c] > maxperc[c])
                maxperc[c] = perc[c];

        putchar('\r');
        //print_vu_meter(perc, maxperc);
        fflush(stdout);
    }
    else if(verbose==3) {
        //printf(_("Max peak (%li samples): 0x%08x "), (long)ocount, max_peak[0]);
        for (val = 0; val < 20; val++)
            if (val <= perc[0] / 5)
                putchar('#');
            else
                putchar(' ');
        printf(" %i%%\n", perc[0]);
        fflush(stdout);
    }
} //compute_max_peak
#endif

/*
 *  read function
 */

static ssize_t pcm_read(u_char *data, size_t rcount)
{
    ssize_t r;
    size_t result = 0;
    size_t count = rcount;
    // FILE *fptr = NULL;

    if (count != chunk_size) {
        count = chunk_size;
    }

    //    fptr = fopen("app_audio_dump.bin", "a");
    //    if(fptr == NULL)
    //    {
    //        printf("fopen is failed\n");
    //    }
    QElapsedTimer audio_read_render_timer_1,audio_read_render_timer_2,audio_read_render_timer_3;


    while (count > 0) {
        audio_read_render_timer_1.start();
        //r = readi_func(handle, data, count);
        r = snd_pcm_readi(handle,data,count);
        //qDebug() << "Total readi_func time : " << audio_read_render_timer_1.elapsed() << "milliseconds";
       // qDebug() << "Count : " << count ;
        if (r == -EAGAIN || (r >= 0 && (size_t)r < count)) {
               //snd_pcm_wait(handle, 1000);
        } else if (r == -EPIPE) {
               //xrun();
        } else if (r == -ESTRPIPE) {
              //suspend();
        } else if (r < 0) {
            error(_("read error: %s"), snd_strerror(r));
            //exit(EXIT_FAILURE);
             return -1;
        }
        /* audio rendering: start */
        int val;
        // fwrite(data,count,sizeof(char),fptr);

        audio_read_render_timer_2.start();
        if ((val = snd_pcm_writei(pcm_handle, data, count)) == -EPIPE)
        {
            //qDebug()<<"XRUN,snd_pcm_writei ";
            snd_pcm_prepare(pcm_handle);
        }
        else if (val < 0)
        {
            qDebug()<<"ERROR. Can't write to PCM device " << snd_strerror(val);
            //printf("ERROR. Can't write to PCM device. %s\n", snd_strerror(val));
            break;
        }
        //qDebug() << "Total readi_func time : " << audio_read_render_timer_2.elapsed() << "milliseconds";
        /* audio rendering stop */
        if(g_b_start == false)
        {
            qDebug()<<" exit from audio inner thread";
            count = 0;
            break;
        }


        if (r > 0) {
            if (vumeter)
                  //compute_max_peak(data, r * hwparams.channels);
                result += r;
            count -= r;
            data += r * bits_per_frame / 8;
        }

        //qDebug() << "Total readi_func and snd_pcm_writei time : " << audio_read_render_timer_1.elapsed() << "milliseconds";
    }
    // fclose(fptr);
    return rcount;
}


/* calculate the data count to read from/to dsp */
static off64_t calc_count(void)
{
    off64_t count;

    if (timelimit == 0) {
        count = pbrec_count;
    } else {
        count = snd_pcm_format_size(hwparams.format, hwparams.rate * hwparams.channels);
        count *= (off64_t)timelimit;
    }
    return count < pbrec_count ? count : pbrec_count;
}


void capture()
{
    //int tostdout=0;		/* boolean which describes output stream */
    // int filecount=0;	/* number of files written */
    //  char namebuf[PATH_MAX+1];
    off64_t count, rest;		/* number of bytes to capture */

    /* get number of bytes to capture */
    count = calc_count();
    if (count == 0)
        count = LLONG_MAX;
    /* WAVE-file should be even (I'm not sure), but wasting one byte
       isn't a problem (this can only be in 8 bit mono) */
    if (count < LLONG_MAX)
        count += count % 2;
    else
        count -= count % 2;

    /* setup sound hardware */

 //   do {

        rest = count;

        /* capture */
        fdcount = 0;
        while (rest > 0 && capture_stop == 0) {
            size_t c = (rest <= (off64_t)chunk_bytes) ?
                        (size_t)rest : chunk_bytes;
            size_t f = c * 8 / bits_per_frame;

            QElapsedTimer audio_read_render_timer;
            audio_read_render_timer.start();

            //qDebug()<<" audio thread running 1";
            if(g_b_start == true)
            {
                if (pcm_read(audiobuf, f) != (ssize_t) f)
                    break;
            }
            else
            {
                qDebug()<<" exit from audio thread";
                break;
            }
            //qDebug()<<" audio thread running 2";

           //qDebug() << "Total audio_read_render_timer time : " << audio_read_render_timer.elapsed() << "milliseconds";
            count -= c;
            rest -= c;
            fdcount += c;
        }


        /* repeat the loop when format is raw without timelimit or
         * requested counts of data are recorded
         */
   // }while ( ((file_type == FORMAT_RAW && !timelimit) || count > 0) && g_b_start == 0);
    //    printf("arecord: Stopping capturing audio.\n");
    //qDebug() << "Stopping capturing audio. : ";
}

#endif //linux
