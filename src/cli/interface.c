#include "interface.h"

/* just call the wavelets, defined in OCaml */
static value ondelette_fun(const value array)
{
    static value *func_ptr = NULL;
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    value res;

    if (!func_ptr)
    {
	pthread_mutex_lock(&mutex);
	func_ptr = caml_named_value("ondelettes algo");
	res = callback(*func_ptr, array);
	pthread_mutex_unlock(&mutex);
    }
    else
	res = callback(*func_ptr, array);
    return res;
}

/* transform a C array to a OCaml bigarray */
static value c_array_to_caml(float *array, const size_t dim)
{
    return alloc_bigarray_dims(BIGARRAY_FLOAT32 | BIGARRAY_C_LAYOUT,
	    1, array, dim);
}

/* return a boolean, that indicate if there is a bucket to compute, and
 * put the bucket to compute in toCompute
 * the ihy field ihy->NbChunk and ihy->DataChunks[i].ChunkSize also
 */
static int get_next_bucket(int *toCompute, ihy_data *ihy)
{
    static int actualBucket = -1;
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    int res;
    int max_bucket = ihy->NbChunk + 1;

    pthread_mutex_lock(&mutex);
    actualBucket++;
    res = actualBucket < max_bucket;
    *toCompute = actualBucket;
    pthread_mutex_unlock(&mutex);
    return res;
}

/* this function assume that ihy is correctly malloc'ed */
static void compute_bucket(const int toCompute, float *arrayf, ihy_data *ihy)
{
    value camlArray;
    int size;

    size = ihy->DataChunks[toCompute].ChunkSize / sizeof(float);
    camlArray = c_array_to_caml(arrayf + (toCompute * NB_BY_O),  size);
    camlArray = ondelette_fun(camlArray);
    memcpy(ihy->DataChunks[toCompute].Values,
	    Data_bigarray_val(camlArray),
	    size);
}

struct thread_data
{
    float *arrayf;
    ihy_data *ihy;
};

/* this function is executed by the thread */
static void *thread_function(void *thread_data)
{
    struct thread_data *data = thread_data;
    int bucket;

    while (get_next_bucket(&bucket, data->ihy))
	compute_bucket(bucket, data->arrayf, data->ihy);
    return NULL;
}

/* just fill out.
 * memcpy is necessary because caml release, with the GC, the memory
 * used by camlArray
 */
static void fill_data(const size_t size, float *arrayf, ihy_data *out)
{
    unsigned int max, i, nbChunk;
    value camlArray;

    max = (size / NB_BY_O);
    nbChunk = ((size / NB_BY_O) + (size % NB_BY_O != 0));
    out->DataChunks = malloc(nbChunk * sizeof(ihy_chunk));
    for (i = 0; i < max; i++)
    {
	/*
	camlArray = c_array_to_caml(arrayf + (i * NB_BY_O), NB_BY_O);
	camlArray = ondelette_fun(camlArray);
	*/
	out->DataChunks[i].ChunkSize = NB_BY_O * sizeof(float);
	out->DataChunks[i].Values = malloc(NB_BY_O * sizeof(float));
	/*
	memcpy(out->DataChunks[i].Values,
		Data_bigarray_val(camlArray),
		NB_BY_O * sizeof(float));
		*/
    };
    out->NbChunk = i - 1;
    if (nbChunk != out->NbChunk)
    {
	/*
	camlArray = c_array_to_caml(arrayf + (i * NB_BY_O),
				    size - (i * NB_BY_O));
	camlArray = ondelette_fun(camlArray);
	*/
	out->NbChunk = i;
	out->DataChunks[i].ChunkSize = (size - (i * NB_BY_O)) * sizeof(float);
	out->DataChunks[i].Values = malloc(out->DataChunks[i].ChunkSize);
	/*
	memcpy(out->DataChunks[i].Values,
		Data_bigarray_val(camlArray),
		out->DataChunks[i].ChunkSize);
		*/
    }
}

/* compute the result of the OCaml function "ondelettes" */
void ondelette(const int8_t *array,
	       const size_t sampleSize,
	       const size_t dim,
	       ihy_data *out)
{
    unsigned int i, j;
    size_t size = dim / sampleSize;
    float *arrayf = malloc (size * sizeof(float));
    int32_t number;
    pthread_t *threads = malloc(NB_THREADS * sizeof(pthread_t));
    struct thread_data dat;

    for (i = 0; i < dim; i += sampleSize)
    {
	j = sampleSize - 1;
	number = array[i];
	/* converting a "reel" sample to float */
	while (j > 0)
	{
	    number = number << 8;
	    number += array[i + j];
	    j--;
	};
	arrayf[i / 2] = number;
    };
    fill_data(size, arrayf, out);
    dat.arrayf = arrayf;
    dat.ihy = out;
    for(i = 0; i < NB_THREADS; i++)
	pthread_create(&threads[i], NULL, thread_function, &dat);
    for (i = 0; i < NB_THREADS; i++)
	pthread_join(threads[i], NULL);
    free(arrayf);
}
