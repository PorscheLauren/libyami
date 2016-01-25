/*
 * Copyright 2016 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <assert.h>
#include "nalReader.h"

namespace YamiParser {

/*get the smallest integer greater than or equal to half of n*/
uint32_t getHalfCeil(uint32_t n)
{
    return ((n + 1) >> 1);
}

NalReader::NalReader(const uint8_t *pdata, uint32_t size)
    : BitReader(pdata, size)
    , m_epb(0)
{
}

inline bool NalReader::isEmulationBytes(const uint8_t *p) const
{
    return *p == 0x03
            && (p - m_stream) >= 2
            && *(p - 1) == 0x00 && *(p - 2) == 0x00;
}

void NalReader::loadDataToCache(uint32_t nbytes)
{
    const uint8_t *pStart = m_stream + m_loadBytes;
    /*the numbers of emulation prevention three byte in current load block*/
    uint32_t epb = 0;

    unsigned long int tmp = 0;
    for (uint32_t i = 0; i < nbytes; i++) {
        if(!isEmulationBytes(pStart + i)) {
            tmp <<= 8;
            tmp |= pStart[i];
        } else {
            epb++;
        }
    }
    m_cache = tmp;
    m_loadBytes += nbytes;
    m_bitsInCache = (nbytes - epb) << 3;
    m_epb += epb;
}

/*according to 9.1 of h264 spec*/
uint32_t NalReader::readUe()
{
    int32_t leadingZeroBits = -1;

    for (uint32_t b = 0; !b; leadingZeroBits++)
        b = read(1);

    return (1 << leadingZeroBits) - 1 + read(leadingZeroBits);
}

/*according to 9.1.1*/
int32_t NalReader::readSe()
{
    uint32_t codeNum = readUe();
    uint32_t ceil = getHalfCeil(codeNum);

    return ((codeNum & 1) ? ceil : (-ceil));
}

bool NalReader::moreRbspData() const
{
    BitReader tmp(*this);
    uint32_t remainingBits = (m_size << 3) - ((m_loadBytes << 3) - m_bitsInCache);
    if(remainingBits == 0)
        return false;

    /* If next bit is equal to 0, that means there is more data in RBSP before the
     * rbsp_trailing_bits() syntax structure.
     * If next bit is equal to 1 and existing another one bit equal to 1 in the
     * remaining bits means more data, otherwise means no more data.
     */
    uint8_t nextBit = tmp.read(1);
    if(nextBit == 0)
        return true;

    while(--remainingBits) {
        nextBit = tmp.read(1);
        if(nextBit)
            return true;
    }

    return false;
}

void NalReader::rbspTrailingBits()
{
    /*rbsp_stop_one_bit, equal to 1*/
    skip(1);
    while(getPos() & 7)
        skip(1); /*rbsp_alignment_zero_bit, equal to 0*/
}

} /*namespace YamiParser*/
