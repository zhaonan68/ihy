#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <caml/mlvalues.h>
#include <caml/callback.h>
#include <caml/bigarray.h>

#include <input/wav.h>
#include <ihy.h>
#include <wavelet.h>
#include <lossless/huffman.h>
#include <output/ao.h>

static void *thread_play_wav(void *wav)
{
    play_wav((wav_data *)wav);
    return NULL;
}

struct thread_data
{
    char	**argv;
    wav_data	*wav;
};

static void *master_thread(void *dat)
{
    struct thread_data *data = dat;
    ihy_data *output;
    huffman_tree *B;
    wav_data *wav;
    unsigned int i;
    int offset;
    size_t size;
    int8_t *encoded;

    /* wavelets */
    printf("Using wavelets on data ... ");
    fflush(stdout);

    output = create_ihy();
    wavelets_direct(data->wav->Data,
	    data->wav->BitsPerSample / 8,
	    data->wav->DataBlocSize,
	    output);
    printf("DONE\n");
    fflush(stdout);

    /* ihy writing */
    printf("Writing ihy file ... ");
    fflush(stdout);
    output->FileID[0] = 'S';
    output->FileID[1] = 'N';
    output->FileID[2] = 'X';
    output->FileID[3] = 'T';
    output->FileSize = 0;
    output->CompressionType = 0;
    output->Channels = data->wav->NumChannels;
    output->Frequency = data->wav->SampleRate;
    output->Artist = malloc(5 * sizeof(char));
    output->Artist[4] = '\0';
    output->ArtistLength = strlen(output->Artist);
    output->Album = malloc(5 * sizeof(char));
    output->Album[4] = '\0';
    output->AlbumLength = strlen(output->Album);
    output->Track = malloc(5 * sizeof(char));
    output->Track[4] = '\0';
    output->TrackLength = strlen(output->Track);
    output->Year = 2009;
    output->Genre = 42;
    output->Comment = malloc(25 * sizeof(char));
    output->Comment[24] = '\0';
    output->CommentLength = strlen(output->Comment);

    write_ihy(output, data->argv[2]);
    printf("DONE\n");

    /* huffman tree */
    printf("Creating Huffman tree ... ");
    fflush(stdout);
    B = build_huffman(data->wav->Data, data->wav->DataBlocSize);
    size = data->wav->DataBlocSize;
    encoded = huffman_encode(data->wav->Data, &size, B);
    printf("size : %d, orig : %d ", size, data->wav->DataBlocSize);
    free(encoded);
    /*huffman_pretty(B, 0, 0);*/
    printf("DONE\n");
    fflush(stdout);
    destroy_huffman(B);

    printf("doing inverse wavelets...");
    fflush(stdout);
    wav = malloc(sizeof(wav_data));
    /* warning hack !! */
    memcpy(wav, data->wav, sizeof(wav_data));
    wav->Data = NULL;
    offset = 0;
    for (i = 0; i < output->NbChunk; i++)
	offset += (output->DataChunks[i].ChunkSize / sizeof(float)) * 2;
    wav->Data = malloc(offset * sizeof(char));
    offset = 0;
    for (i = 0; i < output->NbChunk; i++)
    {
	wavelets_inverse(output->DataChunks[i].Values,
		(output->DataChunks[i].ChunkSize / sizeof(float)),
		wav,
		offset);
	offset += (output->DataChunks[i].ChunkSize / sizeof(float)) * 2;
    };
    printf("DONE\n");

    /*
    for (i = 0; i < data->wav->DataBlocSize; i++)
	printf("org : %d, compr : %d\n", data->wav->Data[i], wav->Data[i]);
	*/

    printf("write wav...");
    fflush(stdout);
    write_wav(wav, data->argv[3]);
    destroy_wav(wav);
    printf("DONE\n");

    destroy_ihy(output);
    return NULL;
}

int main(int argc, char **argv)
{
    caml_main(argv);
    if (argc < 4)
    {
	printf("%s: please specify input and 2 output filename(ihy and wav)\n",
		argv[0]);

	return 1;
    }
    else
    {
	wav_data *input;
	pthread_t play, master;
	struct thread_data dat;

	/* wav reading */
	printf("Loading wav file ... ");
	fflush(stdout);
	input = create_wav();
	read_wav(argv[1], input);
	printf("DONE\n");
	fflush(stdout);

	dat.argv = argv;
	dat.wav = input;
	pthread_create(&play, NULL, thread_play_wav, input);
	pthread_create(&master, NULL, master_thread, &dat);

	pthread_join(play, NULL);
	pthread_join(master, NULL);

	destroy_wav(input);

	return 0;
    }
}
