
#include <alsa/asoundlib.h>
#include <alsa/pcm.h>

#define CLOCK_FREQ INT64_C(1000000)

/* Buffers which arrive in advance of more than AOUT_MAX_PREPARE_TIME
 * will cause the calling thread to sleep */
#define AOUT_MAX_PREPARE_TIME           (2 * CLOCK_FREQ)

/* Buffers which arrive in advance of more than AOUT_MAX_ADVANCE_TIME
 * will be considered as bogus and be trashed */
#define AOUT_MAX_ADVANCE_TIME           (AOUT_MAX_PREPARE_TIME + CLOCK_FREQ)


char *buffer = NULL;
unsigned    i_nb_samples; /* Used for audio */
typedef struct _snd_pcm_hw_params snd_pcm_hw_params_t;
snd_pcm_t *capture_handle ;
snd_pcm_t *playback_pcm;
tatic unsigned int rate = 48000;                       /* stream rate */
static unsigned int channels = 2;                       /* count of channels */
static unsigned int buffer_time = 500000;               /* ring buffer length in us */
static unsigned int period_time = 100000;               /* period time in us */

void audio_capture_render()
{
	size_t bytes;
	int val;
	bytes = snd_pcm_frames_to_bytes (capture_handle, g_period_size);
	val = snd_pcm_start (capture_handle);
	if(val)
	{
		printf("cannot prepare device: %s\n", snd_strerror (val));
		return NULL;
	}
	for (;;)
	{
		
		buffer = (char *)malloc(bytes);
		if(!buffer)
		{
			printf("no memory for audio device \n");
			return -ENOMEM;
		}
		/* Read data */
        snd_pcm_sframes_t frames, delay;
		frames = snd_pcm_readi (capture_handle, buffer, g_period_size);
		if(frames < 0) 
		{
			if (frames == -EAGAIN)
                continue;
			free(buffer)
			buffer = NULL;
			val = snd_pcm_recover (capture_handle, frames, 1);
			if (val == 0)
			{
				printf("cannot read samples: %s",snd_strerror (frames));
				continue;
			}
			printf("cannot recover record stream: %s",snd_strerror (val));
			break;
		
		}
		/* Compute time stamp */
        if (snd_pcm_delay (capture_handle, &delay))
        {
			delay = 0;
		}
        delay += frames;
		i_nb_samples = frames;
		
		while( i_nb_samples > 0)
		{
			snd_pcm_sframes_t r_frames;
			r_frames = snd_pcm_writei (playback_pcm, buffer, i_nb_samples);
			if (r_frames >= 0)
			{
				size_t r_bytes = snd_pcm_frames_to_bytes (playback_pcm, r_frames);
				block->i_nb_samples -= r_frames;
				buffer += r_bytes;
			}
			 else  
			{
				int val = snd_pcm_recover (playback_pcm, r_frames, 1);
				if (val)
				{
				    printf("cannot recover playback stream: %s",snd_strerror (val));
					break;
				}
			   printf("cannot write samples: %s", snd_strerror (frames));
			}		
		}
	}
	free(buffer);
	buffer = NULL;
	return NULL;
}

void audio_render_init()
{
		
 	int val = snd_pcm_open (&playback_pcm, "hw:0,0" , SND_PCM_STREAM_PLAYBACK, 0);
	if( val !=0 )
	{
		printf(" Unable to open sound device for playback\n");
		goto error;
	}
	
	/* Get Initial hardware parameters */
    snd_pcm_hw_params_t *hw;
    unsigned param;
	snd_pcm_hw_params_alloca(&hw);
    snd_pcm_hw_params_any(playback_pcm, hw);
	
	val = snd_pcm_hw_params_set_rate_resample(playback_pcm, hw, 0);
	printf("Initial hardware setup\n");
	
	val = snd_pcm_hw_params_set_rate_resample(playback_pcm, hw, 0);
    if (val)
    {
        printf("cannot disable resampling: %s \n", snd_strerror (val));
        goto error;
    }
	val = snd_pcm_hw_params_set_access (playback_pcm, hw,SND_PCM_ACCESS_RW_INTERLEAVED);
    if (val)
    {
        printf("cannot set access mode: %s", snd_strerror (val));
        goto error;
    }
	
	 /* Set sample format */
    if (snd_pcm_hw_params_test_format (playback_pcm, hw, pcm_format) == 0)
        ;
    else
    if (snd_pcm_hw_params_test_format (playback_pcm, hw, SND_PCM_FORMAT_FLOAT) == 0)
    {
       pcm_format = SND_PCM_FORMAT_FLOAT;
    }
    else
    if (snd_pcm_hw_params_test_format (playback_pcm, hw, SND_PCM_FORMAT_S32) == 0)
    {
        pcm_format = SND_PCM_FORMAT_S32;
    }
    else
    if (snd_pcm_hw_params_test_format (playback_pcm, hw, SND_PCM_FORMAT_S16) == 0)
    {
        pcm_format = SND_PCM_FORMAT_S16;
    }
    else
    {
       printf("no supported sample format");
        goto error;
    }
	val = snd_pcm_hw_params_set_format (playback_pcm, hw, pcm_format);
    if (val)
    {
       printf( "cannot set sample format: %s", snd_strerror (val));
        goto error;
    }
	unsigned channels = 2;
	val = snd_pcm_hw_params_set_channels (playback_pcm, hw, channels);
    if (val)
    {
        printf("cannot set %u channels: %s", channels,snd_strerror (val));
        goto error;
    }
	
	 /* Set sample rate */
	unsigned int i_rate;  
    val = snd_pcm_hw_params_set_rate_near (playback_pcm, hw, &i_rate, NULL);
    if (val)
    {
        printf( "cannot set sample rate: %s", snd_strerror (val));
        goto error;
    }
	/* Set buffer size */
    param = AOUT_MAX_ADVANCE_TIME;
    val = snd_pcm_hw_params_set_buffer_time_near (playback_pcm, hw, &param, NULL);
    if (val)
    {
        printf( "cannot set buffer duration: %s", snd_strerror (val));
        goto error;
    }
	
	param = AOUT_MIN_PREPARE_TIME;
    val = snd_pcm_hw_params_set_period_time_near (playback_pcm, hw, &param, NULL);
    if (val)
    {
        printf( "cannot set period: %s", snd_strerror (val));
        goto error;
    }
	
	 /* Get Initial software parameters */
    snd_pcm_sw_params_t *sw;

    snd_pcm_sw_params_alloca (&sw);
    snd_pcm_sw_params_current (playback_pcm, sw);
	printf(""initial software parameters:\n");
	val = snd_pcm_sw_params_set_start_threshold (playback_pcm, sw, 1);
    if( val < 0 )
    {
        printf( "unable to set start threshold (%s)",snd_strerror( val ) );
        goto error;
    }
	
	/* Commit software parameters. */
    val = snd_pcm_sw_params (playback_pcm, sw);
    if (val)
    {
        printf( "cannot commit software parameters: %s",snd_strerror (val));
        goto error;
    }
	
	val = snd_pcm_prepare (playback_pcm);
    if (val)
    {
        printf( "cannot prepare device: %s", snd_strerror (val));
        goto error;
    }
	return 0;
error:
    snd_pcm_close (playback_pcm);
	
}

void audio_record_init()
{
    int err = 0, dir = 0;
	
    snd_pcm_hw_params_t *hw_params;
    unsigned int rate = 48000;
	const int mode = SND_PCM_NONBLOCK /*| SND_PCM_NO_AUTO_RESAMPLE*/ | SND_PCM_NO_AUTO_CHANNELS /*| SND_PCM_NO_AUTO_FORMAT*/;

    if (( snd_pcm_open (&capture_handle,  "hw:1,0", SND_PCM_STREAM_CAPTURE, mode)) < 0)
    {
        qDebug()<< "cannot open audio device \n"<<err;
		 goto error;
    }
    else
    {
        qDebug()<< " open audio device success \n";
		 goto error;
    }

    if ((err = snd_pcm_hw_params_alloca (&hw_params)) < 0)
    {
        qDebug()<< "cannot allocate hardware parameter structure";
		 goto error;
    }

    if ((err = snd_pcm_hw_params_any (capture_handle, hw_params)) < 0)
    {
        qDebug()<<  "cannot initialize hardware parameter structure ";
		 goto error;
    }
	int val = snd_pcm_hw_params_set_rate_resample (capture_handle, hw_params, 0);
	if (val)
	{
		qDebug()<<  "cannot disable resampling: ";
		 goto error;
	}
	

    if ((err = snd_pcm_hw_params_set_access (capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
    {
         qDebug()<<  "cannot set access type ";
		  goto error;
    }

	val = snd_pcm_hw_params_set_format(capture_handle, hw_params, SND_PCM_FORMAT_S16_LE)
	if(val)
	{
		qDebug()<<  "cannot set format ";
		 goto error;
	}
	 /* set the count of channels */
	err = snd_pcm_hw_params_set_channels(capture_handle, hw_params, channels);
	if (err < 0) 
	{
			printf("Channels count (%i) not available for playbacks: %s\n", channels, snd_strerror(err));
			goto error;
	}
	/* set the buffer time */
	err = snd_pcm_hw_params_set_buffer_time_near(capture_handle, hw_params, &buffer_time, &dir);
	if (err < 0) 
	{
			printf("Unable to set buffer time %i for playback: %s\n", buffer_time, snd_strerror(err));
			goto error;
	}
	/* set the period time */
	err = snd_pcm_hw_params_set_period_time_near(capture_handle, hw_params, &period_time, &dir);
	if (err < 0) {
			printf("Unable to set period time %i for playback: %s\n", period_time, snd_strerror(err));
			return err;
	}
	
	val = snd_pcm_hw_params_get_period_size (hw_params, &g_period_size, &dir);
	if(val)
	{
		qDebug()<<  "cannot get g_period_size ";
		 goto error;
	}
	qDebug()<<"g_period_size "<<g_period_size;
	if (dir > 0)
	{
		g_period_size++;
		qDebug()<<"g_period_size++ "<<g_period_size;
	}
#if 0 
    if ((err = snd_pcm_hw_params_set_rate_near (capture_handle, hw_params, &rate, 0)) < 0)
    {
      qDebug()<<   "cannot set sample rate ";
	  goto error;
    }
#endif
	/* Commit hardware parameters */
    if ((err = snd_pcm_hw_params (capture_handle, hw_params)) < 0)
    {
          qDebug()<<"cannot set parameters ";
		  goto error;
    }
error:
	qDebug()<<"error occurred ";
	snd_pcm_close(capture_handle);
}

int main()
{
	audio_record_init();
	audio_render_init();
	audio_capture_render();
	
	return 0;

}