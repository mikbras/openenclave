#include "bitmap.h"

static void _hex_dump(const uint8_t* data, uint32_t size, bool printables)
{
    uint32_t i;

    printf("%u bytes\n", size);

    for (i = 0; i < size; i++)
    {
        unsigned char c = data[i];

        if (printables && (c >= ' ' && c < '~'))
            printf("'%c'", c);
        else
            printf("%02X", c);

        if ((i + 1) % 16)
        {
            printf(" ");
        }
        else
        {
            printf("\n");
        }
    }

    printf("\n");
}

bool bitmap_zero_filled(const uint8_t* data, uint32_t size)
{
    uint32_t i;

    for (i = 0; i < size; i++)
    {
        if (data[i])
            return 0;
    }

    return 1;
}

uint32_t bitmap_count_bits(uint8_t byte)
{
    uint8_t i;
    uint8_t n = 0;

    for (i = 0; i < 8; i++)
    {
        if (byte & (1 << i))
            n++;
    }

    return n;
}

uint32_t bitmap_count_bits_n(const uint8_t* data, uint32_t size)
{
    uint32_t i;
    uint32_t n = 0;

    for (i = 0; i < size; i++)
    {
        n += bitmap_count_bits(data[i]);
    }

    return n;
}

void bitmap_dump(const uint8_t* data, uint32_t size)
{
    if (bitmap_zero_filled(data, size))
    {
        printf("...\n\n");
    }
    else
    {
        _hex_dump(data, size, 0);
    }
}
