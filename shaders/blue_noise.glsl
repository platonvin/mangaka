uint part1by1(uint x) {
    x = (x & 0x0000ffffu);
    x = ((x ^ (x << 8u)) & 0x00ff00ffu);
    x = ((x ^ (x << 4u)) & 0x0f0f0f0fu);
    x = ((x ^ (x << 2u)) & 0x33333333u);
    x = ((x ^ (x << 1u)) & 0x55555555u);
    return x;
}

uint compact1by1(uint x) {
    x = (x & 0x55555555u);
    x = ((x ^ (x >> 1u)) & 0x33333333u);
    x = ((x ^ (x >> 2u)) & 0x0f0f0f0fu);
    x = ((x ^ (x >> 4u)) & 0x00ff00ffu);
    x = ((x ^ (x >> 8u)) & 0x0000ffffu);
    return x;
}

uint pack_morton2x16(uvec2 v) {
    return part1by1(v.x) | (part1by1(v.y) << 1);
}

uvec2 unpack_morton2x16(uint p) {
    return uvec2(compact1by1(p), compact1by1(p >> 1));
}

uint inverse_gray32(uint n) {
    n = n ^ (n >> 1);
    n = n ^ (n >> 2);
    n = n ^ (n >> 4);
    n = n ^ (n >> 8);
    n = n ^ (n >> 16);
    return n;
}

// https://www.shadertoy.com/view/llGcDm
int hilbert(ivec2 p, int level)
{
    int d = 0;
    for (int k = 0; k < level; k++)
    {
        int n = level - k - 1;
        ivec2 r = (p >> n) & 1;
        d += ((3 * r.x) ^ r.y) << (2 * n);
        if (r.y == 0) {
            if (r.x == 1) {
                p = (1 << n) - 1 - p;
            }
            p = p.yx;
        }
    }
    return d;
}

// https://www.shadertoy.com/view/llGcDm
ivec2 ihilbert(int i, int level)
{
    ivec2 p = ivec2(0, 0);
    for (int k = 0; k < level; k++)
    {
        ivec2 r = ivec2(i >> 1, i ^ (i >> 1)) & 1;
        if (r.y == 0) {
            if (r.x == 1) {
                p = (1 << k) - 1 - p;
            }
            p = p.yx;
        }
        p += r << k;
        i >>= 2;
    }
    return p;
}

// knuth's multiplicative hash function (fixed point R1)
uint kmhf(uint x) {
    return 0x80000000u + 2654435789u * x;
}

uint kmhf_inv(uint x) {
    return (x - 0x80000000u) * 827988741u;
}

// mapping each pixel to a hilbert curve index, then taking a value from the Roberts R1 quasirandom sequence for it
uint hilbert_r1_blue_noise(uvec2 p) {
    #if 1
    uint x = uint(hilbert(ivec2(p), 17)) % (1u << 17u);
    #else
    //p = p ^ (p >> 1);
    uint x = pack_morton2x16(p) % (1u << 17u);
    //x = x ^ (x >> 1);
    x = inverse_gray32(x);
    #endif
    #if 0
    // based on http://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/
    const float phi = 2.0 / (sqrt(5.0) + 1.0);
    return fract(0.5 + phi * float(x));
    #else
    x = kmhf(x);
    return x;
    #endif
}

// mapping each pixel to a hilbert curve index, then taking a value from the Roberts R1 quasirandom sequence for it
float hilbert_r1_blue_noisef(uvec2 p) {
    uint x = hilbert_r1_blue_noise(p);
    #if 0
    return float(x >> 24) / 256.0;
    #else
    return float(x) / 4294967296.0;
    #endif
}

// inverse
uvec2 hilbert_r1_blue_noise_inv(uint x) {
    x = kmhf_inv(x);
    return uvec2(ihilbert(int(x), 17));
}
