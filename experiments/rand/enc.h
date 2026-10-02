#ifndef KS_RAND_ENC_H
#define KS_RAND_ENC_H

#include <cstddef>
#include <vector>

using Block = std::vector<unsigned char>;

/**
 * @brief Perform a Reed-Solomon encoding of a buffer.
 * @param buffer the buffer.
 * @param size the size of \p buffer.
 * @param t the error threshold.
 * @param n the number of codewords.
 * @return the code words.
 */
std::vector<Block> encode(const unsigned char* buffer,
                          std::size_t size,
                          std::size_t t,
                          std::size_t n);

/**
 * @brief Decode (in a naive fashion) a previously encoded set of blocks.
 * @param blocks the blocks.
 * @param ids the block indices.
 * @param t the threshold.
 * @param n the total number of blocks.
 */
std::vector<unsigned char> decode(const std::vector<Block>& blocks,
                                  const std::vector<std::size_t>& ids,
                                  std::size_t t);

#endif  // KS_RAND_ENC_H
