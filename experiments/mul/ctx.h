#ifndef KS_MUL_CTX_H
#define KS_MUL_CTX_H

#include <unordered_map>
#include <vector>

#include <scl/math/vector.h>
#include <scl/net/packet.h>

#include "util.h"

using MsgId = unsigned char;

constexpr static MsgId MSG1 = 1;
constexpr static MsgId MSG2 = 2;

inline bool checkMsgId(scl::net::Packet& packet, MsgId msg_id) {
  return packet.read<MsgId>() == msg_id;
}

class Context {
 public:
  static void init(std::size_t number_of_parties, std::size_t psi);

  static void print() {
    std::cout << "numberOfParties = " << m_number_of_parties << "\n";
    std::cout << "threshold = " << m_threshold << "\n";
    std::cout << "|G| = " << m_generators.size() << "\n";
    std::cout << "|thetas| = " << m_thetas.size() << "\n";
    std::cout << "mu = " << m_mu << "\n";
    std::cout << "psi = " << m_psi << "\n";
  }

  // number of parties in the computation
  static std::size_t numberOfParties() {
    return m_number_of_parties;
  }

  // corruption threshold
  static std::size_t threshold() {
    return m_threshold;
  }

  // fixed generators used in the APOCM proof
  static const std::vector<Curve>& generators() {
    return m_generators;
  }

  // a particular fixed generator
  static Curve generator(std::size_t index) {
    return m_generators[index];
  }

  // a particular theta for a party
  static Field theta(std::size_t party_id) {
    return m_thetas[party_id];
  }

  // the thetas (\theta_1, ..., \theta_n)
  static const std::vector<Field>& thetas() {
    return m_thetas;
  }

  // powers of a particular theta. Used in the APOCM proof:
  // thetas(i) := (\theta_i^1, ..., \theta_i^t)
  static const std::vector<Field>& thetas(std::size_t i) {
    return m_pow_thetas[i];
  }

  // lagrange basis for computing some polynomials in the APOCM proof
  static const scl::math::Vector<Field>& lagrangeBasisT(std::size_t i) {
    return m_lagrange_basis[i];
  }

  // mu and psi together determine the compression rate of the APOCM
  // proof. Higher psi implies a larger proof, but less work in
  // proving/verifying the proof.

  // mu parameter of the APOCM proof.
  static std::size_t mu() {
    return m_mu;
  }

  // psi parameter of the APOCM proof.
  static std::size_t psi() {
    return m_psi;
  }

 private:
  Context() {}

  static std::size_t m_number_of_parties;
  static std::size_t m_threshold;

  static std::vector<Curve> m_generators;
  static std::vector<Field> m_thetas;
  static std::vector<std::vector<Field>> m_pow_thetas;
  static std::unordered_map<std::size_t, scl::math::Vector<Field>>
      m_lagrange_basis;

  // #parties = m_mu * m_psi
  static std::size_t m_mu;
  static std::size_t m_psi;
};

#endif  // KS_MUL_CTX_H
