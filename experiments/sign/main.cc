#include <fstream>
#include <iostream>
#include <memory>
#include <unordered_map>

#include <scl/simulation/context.h>
#include <scl/simulation/simulator.h>
#include <scl/util/cmdline.h>
#include <scl/util/digest.h>
#include <scl/util/sha256.h>

#include "manager.h"

using namespace scl;

namespace scl::util {

template <>
float ProgramOptions::get<float>(std::string_view name) const {
  const auto m = (float)std::atof(m_args.at(name).data());
  if (m < 0 || m > 1.0) {
    std::cout << "invalid value for 'm'\n";
    std::exit(1);
  }
  return m;
}

}  // namespace scl::util

namespace {

struct CancelSimplePartiesHook final : public sim::Hook {
  void run(std::size_t party_id, const sim::SimulationContext& ctx) override {
    if (party_id == 0) {
      ctx.cancelSimulation();
    }
  }
};

}  // namespace

int main(int argc, char** argv) {
  const auto args =
      util::ProgramOptions::Parser("Sign protocol simulation")
          .add(util::ProgramArg::required("n", "size_t", "number of parties"))
          .add(
              util::ProgramArg::optional("o", "string", {}, "output directory"))
          .add(util::ProgramFlag("simple",
                                 "only party 0 recovers the signature"))
          .add(util::ProgramFlag("check", "check correctness of output"))
          .parse(argc, argv);

  const auto n = args.get<std::size_t>("n");
  const auto simple = args.flagSet("simple");
  const auto check = args.flagSet("check");

  const auto message = util::Sha256{}.update(123).finalize();

  std::unique_ptr<Manager> manager;
  std::ofstream f;

  if (args.has("o")) {
    f = std::ofstream(std::string(args.get("o")), std::ios::app);
    if (!f.is_open()) {
      std::cerr << "could not open file for output\n";
      std::exit(1);
    }

    manager = std::make_unique<Manager>(f, message, n, simple, check);
  } else {
    manager = std::make_unique<Manager>(std::cout, message, n, simple, check);
  }

  if (simple) {
    manager->addHook<CancelSimplePartiesHook>(sim::EventType::STOP);
  }

  sim::simulate(std::move(manager));

  if (args.has("o")) {
    f.close();
  }
}
