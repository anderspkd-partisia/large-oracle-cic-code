#include <fstream>

#include <scl/scl.h>
#include <scl/simulation/context.h>
#include <scl/simulation/simulator.h>

#include "manager.h"
#include "pbcts.h"
#include "protocol.h"

using namespace scl;

namespace {

struct DebugHook final : public sim::Hook {
  DebugHook(std::size_t id) : id(id) {}
  void run(std::size_t party_id, const sim::SimulationContext& ctx) override {
    if (party_id == id) {
      std::cout << party_id << ": " << ctx.trace(party_id).back() << "\n";
    }
  }

  std::size_t id;
};

}  // namespace

int main(int argc, char** argv) {
  const auto args =
      util::ProgramOptions::Parser("PBCTS protocol simulation")
          .add(util::ProgramArg::required("n", "uint", "number of parties"))
          .add(util::ProgramArg::optional("m",
                                          "uint",
                                          "100",
                                          "number of multiplications"))
          .add(
              util::ProgramArg::optional("o", "string", {}, "output directory"))
          .add(util::ProgramFlag("debug", "add debugging"))
          .parse(argc, argv);

  const auto n = args.get<std::size_t>("n");
  const auto m = args.get<std::size_t>("m");

  std::unique_ptr<sim::Manager> man;
  std::ofstream f;

  if (args.has("o")) {
    f = std::ofstream(std::string(args.get("o")), std::ios::app);
    man = Manager::create(f, n, m);
  } else {
    man = Manager::create(std::cout, n, m);
  }

  if (args.flagSet("debug")) {
    man->addHook<DebugHook>(0);
  }

  sim::simulate(std::move(man));
}
