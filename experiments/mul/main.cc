#include <chrono>
#include <fstream>
#include <random>
#include <ratio>

#include <scl/scl.h>
#include <scl/simulation/context.h>

#include "ctx.h"
#include "manager.h"

using namespace scl;

namespace {

struct DebugHook final : public sim::Hook {
  DebugHook(std::size_t id, std::size_t n) : id(id), n(n) {}
  void run(std::size_t party_id, const sim::SimulationContext& ctx) override {
    std::cout << party_id << ": " << ctx.trace(party_id).back() << "\n";

    // std::cout << "[";
    // for (std::size_t i = 0; i < n; i++) {
    //   std::cout << ctx.trace(i).size() << " ";
    // }
    // std::cout << "]\n";
  }

  std::size_t id;
  std::size_t n;
};

}  // namespace

int main(int argc, char** argv) {
  const auto args =
      util::ProgramOptions::Parser("Mul protocol simulation")
          .add(util::ProgramArg::required("n", "uint", "number of parties"))
          .add(util::ProgramArg::required("psi", "uint", "psi"))
          .add(util::ProgramArg::optional("m",
                                          "uint",
                                          "100",
                                          "number of multiplications"))
          .add(
              util::ProgramArg::optional("o", "string", {}, "output directory"))
          .add(util::ProgramFlag("check", "check output for correctness"))
          .add(util::ProgramFlag("fake_zk", "use a heuristic for ZK proofs"))
          .add(util::ProgramFlag("debug",
                                 "print events as they are being generated"))
          .parse(argc, argv);

  const auto n = args.get<std::size_t>("n");
  const auto m = args.get<std::size_t>("m");
  const auto psi = args.get<std::size_t>("psi");

  const auto check = args.flagSet("check");
  const auto fake_zk = args.flagSet("fake_zk");

  Context::init(n, psi);

  if (args.has("o")) {
    std::ofstream f(std::string(args.get("o")), std::ios::app);
    if (!f.is_open()) {
      std::cerr << "could not open file for output\n";
      std::exit(1);
    }

    auto man = std::make_unique<Manager>(f, n, m, check, fake_zk);

    if (args.flagSet("debug")) {
      man->addHook<DebugHook>(0, n);
    }

    sim::simulate(std::move(man));
  } else {
    auto man = std::make_unique<Manager>(std::cout, n, m, check, fake_zk);

    if (args.flagSet("debug")) {
     man->addHook<DebugHook>(0, n);
    }

    sim::simulate(std::move(man));
  }
}
