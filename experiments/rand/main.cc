#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ratio>

#include <scl/simulation/simulator.h>
#include <scl/util/cmdline.h>

#include "ctx.h"
#include "manager.h"
#include "network.h"

using namespace scl;

namespace {

struct DebugHook final : public sim::Hook {
  DebugHook(std::size_t id, std::size_t n) : id(id), n(n) {}
  void run(std::size_t party_id, const sim::SimulationContext& ctx) override {
    if (party_id < n) {
      std::cout << party_id << ": " << ctx.trace(party_id).back() << "\n";
    }

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
      util::ProgramOptions::Parser("Rand protocol simulation")
          .add(util::ProgramArg::required("n", "uint", "number of parties"))
          .add(
              util::ProgramArg::optional("m", "uint", "100", "number of rands"))
          .add(
              util::ProgramArg::optional("o", "string", {}, "output directory"))
          .add(util::ProgramFlag("debug", "print events"))
          .add(util::ProgramFlag("check", "check output for correctness"))
          .parse(argc, argv);

  const auto n = args.get<std::size_t>("n");
  const auto m = args.get<std::size_t>("m");
  const auto check = args.flagSet("check");

  Context::init(n, m);

  std::unique_ptr<sim::Manager> manager;
  std::ofstream f;

  if (args.has("o")) {
    f = std::ofstream(std::string(args.get("o")), std::ios::app);
    if (!f.is_open()) {
      std::cerr << "could not open file for output\n";
      std::exit(1);
    }

    manager = Manager::create(f, n, m, check);
  } else {
    manager = Manager::create(std::cout, n, m, check);
  }

  manager->addHook<CancelBcHook>(sim::EventType::STOP, n);

  if (args.flagSet("debug")) {
    manager->addHook<DebugHook>(0, n);
  }

  sim::simulate(std::move(manager));

  if (args.has("o")) {
    f.close();
  }
}
