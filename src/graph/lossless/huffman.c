#include "huffman.h"

/* just a simple swap */
static void swap(huffman_tree *a, huffman_tree *b)
{
    huffman_tree c;
    c = *a;
    *a = *b;
    *b = c;
}

/* structure for a heap */
typedef struct heap
{
    size_t		size;
    huffman_tree	**array; /* array of huffman_tree * */
} heap;

/* add an element to the heap, on the right place */
static void heap_add(heap *heap, huffman_tree *B)
{
    int i;

    i = heap->size;
    heap->array[i] = B;
    while (i > 0 &&
	    heap->array[i]->occurrence < heap->array[(i - 1) / 2]->occurrence)
    {
	swap(heap->array[i], heap->array[(i - 1) / 2]);
	i = (i - 1) / 2;
    };
    heap->size++;
}

/* remove the root of the heap, and put it on out */
static huffman_tree *heap_rm(heap *heap)
{
    huffman_tree *res;
    unsigned int i, min, l, r;

    res = heap->array[0];
    heap->size--;
    heap->array[0] = heap->array[heap->size];
    heap->array[heap->size] = NULL;
    i = 0;
    while (i < heap->size)
    {
	min = i;
	l = (i * 2) + 1;
	r = (i * 2) + 2;
	if (l < heap->size &&
		heap->array[i]->occurrence > heap->array[l]->occurrence)
	    min = l;
	if (r < heap->size &&
		heap->array[min]->occurrence > heap->array[r]->occurrence)
	    min = r;
	if (min != i)
	{
	    swap(heap->array[i], heap->array[min]);
	    i = min;
	}
	else
	    i = heap->size; /* force the loop to end */
    };
    return res;
}

static char *to_binary(unsigned int code, int code_size)
{
    char *res = malloc(code_size * sizeof(char));
    int i;

    for (i = 0; i < code_size; i++)
    {
	code_size--;
	sprintf(res + i, "%d", code >> code_size);
	code = code - ((code >> code_size) << code_size);
    }
    return res;
}

/* pretty print huffman */
void huffman_pretty(huffman_tree *B, int code, int code_size)
{
    if (B)
    {
	if (!B->fg)
	{
	    char *bin = to_binary(code, code_size);
	    printf("\n");
	    printf("%c, %d, %s", B->letter, B->occurrence, bin);
	    free(bin);
	}
	else
	{
	    huffman_pretty(B->fg, (code << 1) + 0, code_size + 1);
	    huffman_pretty(B->fd, (code << 1) + 1, code_size + 1);
	}
    }
}

/* build the huffman tree, and return it */
huffman_tree *build_huffman(const void *varray, const size_t n)
{
    unsigned int i;
    huffman_tree **tree;
    huffman_tree *father;
    int8_t *array = (int8_t *)varray;
    heap H;

    tree = malloc(256 * sizeof(huffman_tree *));
    for (i = 0; i < 256; i++)
    {
	tree[i] = malloc(sizeof(huffman_tree));
	tree[i]->letter = i;
	tree[i]->occurrence = 0;
	tree[i]->fd = NULL;
	tree[i]->fg = NULL;
    };
    for (i = 0; i < n; i++)
	tree[(unsigned char) array[i]]->occurrence++;

    /* put in the heap */
    H.size = 0;
    H.array = malloc(256 * sizeof(huffman_tree *));
    for (i = 0; i < 256; i++)
	heap_add(&H, tree[i]);

    /* build the huffman tree */
    while (H.size > 1)
    {
	father = malloc(sizeof(huffman_tree));
	father->fd = heap_rm(&H);
	father->fg = heap_rm(&H);
	father->occurrence = father->fd->occurrence + father->fg->occurrence;
	heap_add(&H, father);
    }
    father = heap_rm(&H);
    free(tree);
    free(H.array);
    return father;
}

static void parc_prof_huffman(huffman_tree *H, int code, int length)
{
    if (H)
    {
	H->code = code;
	H->codelength = length;
	parc_prof_huffman(H->fg, (code << 1), length + 1);
	parc_prof_huffman(H->fd, (code << 1) + 1, length + 1);
    }
}

struct huffman_code
{
    unsigned int	code;
    int			length;
};

static void build_code(huffman_tree *H, struct huffman_code *code)
{
    if (H)
    {
	if (!H->fg)
	{
	    code[H->letter].code = H->code;
	    code[H->letter].length = H->codelength;
	};
	build_code(H->fg, code);
	build_code(H->fd, code);
    }
}

/* write the huffman tree, before the encoded values */
static void huffman_write_tree(huffman_tree *H, int8_t **pos)
{
    t_queue F;
    huffman_tree *tmp;

    F = queue_create();
    queue_enqueue(H, F);
    while (!queue_isempty(F))
    {
	tmp = (huffman_tree *)queue_dequeue(F);
	if (!tmp->fg)
	{
	    **pos = 0; /* this is not a leaf */
	    (*pos)++;
	    **pos = 0;
	    (*pos)++;
	    queue_enqueue(tmp->fg, F);
	    queue_enqueue(tmp->fd, F);
	}
	else
	{
	    **pos = 1; /* this is a leaf */
	    (*pos)++;
	    **pos = tmp->letter;
	    (*pos)++;
	}
    }
    queue_destroy(F);
}

/* encode the data
 * n is the size of varray and will be, when the function returns, the
 * size of the array returned
 */
int8_t *huffman_encode(const void *varray, size_t *n, huffman_tree *H)
{
    struct huffman_code code[256];
    struct huffman_code letter_code;
    int8_t *res = calloc(*n, 1);
    int8_t *sentry = res;
    uint8_t *array = (uint8_t *)varray;
    int shift;
    unsigned int i;
    int tmp;

    parc_prof_huffman(H, 0, 0);
    build_code(H, code);
    huffman_write_tree(H, &sentry);
    shift = 0;
    for (i = 0; i < *n; i++)
    {
	/*printf("i : %d\n", i);*/
	letter_code = code[array[i]];
	while (letter_code.length > 0)
	{
	    /*printf("length : %d\n", letter_code.length);*/
	    *sentry |= letter_code.code << (8 - shift - letter_code.length);
	    tmp = shift;
	    if (letter_code.length > (8 - shift))
	    {
		sentry++;
		shift = 0;
	    }
	    else
		shift = (shift + letter_code.length) % 8;
	    /*
	    printf("shift : %d\n", shift);
	    printf("8 - tmp : %d\n", 8 - tmp);
	    */
	    letter_code.length -= (8 - tmp);
	};
    }
    *n = (sentry - res) + (shift != 0);
    res = realloc(res, *n);
    return res;
}

/* release the memory used by a huffman tree */
void destroy_huffman(huffman_tree *B)
{
    if (B)
    {
	destroy_huffman(B->fd);
	destroy_huffman(B->fg);
	free(B);
    }
}