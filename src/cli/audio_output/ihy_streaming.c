#include "ihy_streaming.h"

struct ihy_streaming_data
{
    t_buffer	buffer;
    ihy_data	*ihy;
    int		*has_filled_the_buffer;
};

struct ihy_buffer_content
{
    int8_t	*samples;
    uint32_t	samplesSize;
};

static void *ihy_playing(void *data)
{
    /* contains uncompressed data, (ihy_buffer_content *) */
    t_buffer buffer = ((struct ihy_streaming_data *)data)->buffer;
    ihy_data *ihy = ((struct ihy_streaming_data *)data)->ihy;
    ao_device *audio_device;
    struct ihy_buffer_content *to_play;
    int *can_play = ((struct ihy_streaming_data *)data)->has_filled_the_buffer;

    audio_device = ao_init_device(16, ihy->Channels, ihy->Frequency);

    /* we need to ensure that the other thread has added,
     * at least, 1 element
     */
    while (!*can_play)
	usleep(50);
    while (!buffer_isempty(buffer))
    {
	to_play = buffer_get(buffer);
	ao_play_samples(audio_device, to_play->samples, to_play->samplesSize);
	free(to_play); /* hum quite dangerous */
    }
    ao_close_device(audio_device);
    return NULL;
}

static void *ihy_filling_buffer(void *data)
{
    /* contains uncompressed data, (ihy_buffer_content *) */
    t_buffer buffer = ((struct ihy_streaming_data *)data)->buffer;
    ihy_data *ihy = ((struct ihy_streaming_data *)data)->ihy;
    struct ihy_buffer_content *to_add;
    unsigned int i = 0;

    while (i < ihy->NbChunk)
    {
	to_add = malloc(sizeof(struct ihy_buffer_content));
	to_add->samplesSize = ihy->DataChunks[i].ChunkSize;
	to_add->samples =
	    huffman_decode(ihy->DataChunks[i].Values, &to_add->samplesSize);
	wavelets_inverse((float *)to_add->samples,
			 to_add->samplesSize / sizeof(float),
			 to_add->samples,
			 0);
	to_add->samplesSize = to_add->samplesSize / 2;
	buffer_add(to_add, buffer);
	if (!i) /* i == 0 */
	    *(((struct ihy_streaming_data *)data)->has_filled_the_buffer) = 1;
	i++;
    }
    return NULL;
}

void play_ihy_streaming(ihy_data *ihy)
{
    t_buffer buffer; /* contains ihy_buffer_content */
    pthread_t playing_thread, filling_buffer_thread;
    struct ihy_streaming_data data;
    int has_filled_the_buffer = 0;

    buffer = buffer_init(10); /* max 10 elements */
    data.ihy = ihy;
    data.buffer = buffer;
    data.has_filled_the_buffer = &has_filled_the_buffer;
    pthread_create(&filling_buffer_thread, NULL, &ihy_filling_buffer, &data);
    pthread_create(&playing_thread, NULL, &ihy_playing, &data);
    pthread_join(filling_buffer_thread, NULL);
    pthread_join(playing_thread, NULL);
}
