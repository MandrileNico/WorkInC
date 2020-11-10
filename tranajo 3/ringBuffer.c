/*==================[inclusions]=============================================*/
#include "ringBuffer.h"
#include "stdlib.h"

/*==================[macros and definitions]=================================*/

typedef struct
{
    int32_t indexRead;
    int32_t indexWrite;
    int32_t count;
    int32_t size;
    uint8_t *pBuf;
}ringBuferData_struct;

/*==================[internal data declaration]==============================*/

/*==================[internal functions declaration]=========================*/

/*==================[internal data definition]===============================*/

/*==================[external data definition]===============================*/

/*==================[internal functions definition]==========================*/

/*==================[external functions definition]==========================*/

void *ringBuffer_init(int32_t size)
{
    ringBuferData_struct *rb;

    /* TODO: verificar puntero null*/
    rb = malloc(sizeof(ringBuferData_struct));

    /* TODO: verificar puntero null*/
    rb->pBuf = malloc(size);

    rb->indexRead = 0;
    rb->indexWrite = 0;
    rb->count = 0;
    rb->size = size;

    return rb;
}

void ringBuffer_deInit(void *rb)
{
    // TODO: implementar
}

bool ringBuffer_putData(void *pRb, uint8_t data)
{
    ringBuferData_struct *rb = pRb;
    bool ret = true;

    rb->pBuf[rb->indexWrite] = data;

    rb->indexWrite++;
    if (rb->indexWrite >= rb->size)
        rb->indexWrite = 0;

    if (rb->count < rb->size)
    {
        rb->count++;
    }
    else
    {
        /* si el buffer está lleno incrementa en uno indexRead
         * haciendo que se pierda el dato más viejo y devuelve
         * true para indicar que se estan perdiendo datos */
        rb->indexRead++;
        if (rb->indexRead >= rb->size)
            rb->indexRead = 0;
        ret = false;
    }

    return ret;
}

bool ringBuffer_getData(void *pRb, uint8_t *data)
{
    ringBuferData_struct *rb = pRb;
    bool ret = true;

    if (rb->count)
    {
        *data = rb->pBuf[rb->indexRead];

        rb->indexRead++;
        if (rb->indexRead >= rb->size)
            rb->indexRead = 0;
        rb->count--;
    }
    else
    {
        ret = false;
    }

    return ret;
}

bool ringBuffer_isFull(void *pRb)
{
    ringBuferData_struct *rb = pRb;

    return rb->count == rb->size;
}

bool ringBuffer_isEmpty(void *pRb)
{
    ringBuferData_struct *rb = pRb;

    return rb->count == 0;
}

/*==================[end of file]============================================*/
