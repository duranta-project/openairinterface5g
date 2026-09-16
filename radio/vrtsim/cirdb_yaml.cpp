/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "cirdb_yaml.h"
#include <yaml-cpp/yaml.h>
#include <cmath>
#include <string>
extern "C" {
#include "common/utils/LOG/log.h"
}

static inline double dabs(double x)
{
  return x < 0 ? -x : x;
}

int cirdb_yaml_scan(const char *yaml_path)
{
  YAML::Node root;
  try {
    root = YAML::LoadFile(yaml_path);
  } catch (const YAML::Exception &e) {
    LOG_E(HW, "CIRDB YAML parse error in %s: %s\n", yaml_path, e.what());
    return -1;
  }
  auto ents = root["entries"];
  if (!ents || !ents.IsSequence()) {
    LOG_E(HW, "CIRDB YAML: missing or invalid 'entries' in %s\n", yaml_path);
    return -1;
  }

  int n = static_cast<int>(ents.size());
  LOG_I(HW, "CIRDB YAML: %d entries in %s\n", n, yaml_path);
  return n;
}

int cirdb_yaml_select(const cirdb_select_req_t *req, cirdb_entry_meta_t *out)
{
  if (!req || !out || !req->yaml_path) {
    LOG_E(HW, "CIRDB YAML: invalid arguments to cirdb_yaml_select\n");
    return -1;
  }

  YAML::Node root;
  try {
    root = YAML::LoadFile(req->yaml_path);
  } catch (const YAML::Exception &e) {
    LOG_E(HW, "CIRDB YAML parse error in %s: %s\n", req->yaml_path, e.what());
    return -1;
  }

  auto ents = root["entries"];
  if (!ents || !ents.IsSequence()) {
    LOG_E(HW, "CIRDB YAML: missing or invalid 'entries' in %s\n", req->yaml_path);
    return -1;
  }

  bool found = false;
  double best_cost = 1e300;
  cirdb_entry_meta_t best{};

  // Determine if specific parameters are required
  bool want_specific_model = (req->want_model_id >= 0);
  bool want_specific_antennas = (req->want_tx > 0 && req->want_rx > 0);

  for (std::size_t i = 0; i < ents.size(); ++i) {
    const YAML::Node e = ents[i];
    int pair_order = e["pair_order"].as<int>(0);

    int model_id = e["model_id"].as<int>(-1);

    // STRICT: model_id must match if specified
    if (want_specific_model && model_id != req->want_model_id)
      continue;

    int n_tx = e["n_tx"].as<int>(0);
    int n_rx = e["n_rx"].as<int>(0);

    // STRICT: antenna dimensions must match exactly if specified
    bool shape_ok = true;
    if (want_specific_antennas) {
      shape_ok = (n_tx == req->want_tx && n_rx == req->want_rx);
      bool shape_swap_ok = req->allow_shape_swap ? (n_tx == req->want_rx && n_rx == req->want_tx) : false;
      shape_ok = shape_ok || shape_swap_ok;

      if (!shape_ok)
        continue; // Skip entries with wrong antenna configuration
    }

    // STRICT: antenna dimensions must match exactly if specified
    if (want_specific_antennas) {
      if (n_tx != req->want_tx || n_rx != req->want_rx)
        continue; // Skip entries with wrong antenna configuration
    }

    float ds_ns = e["ds_ns"].as<float>(0.0f);
    float sp = e["speed_mps"].as<float>(0.0f);
    float aoa = e["los_aoa_deg"].as<float>(0.0f);

    // STRICT: AoA exact match when entry has los_aoa_deg field
    if (e["los_aoa_deg"].IsDefined() && dabs(aoa - req->want_aoa_deg) > 0.01)
      continue;

    // FLEXIBLE: delay spread and speed - find closest match using cost function
    double cds = 0.0;
    double csp = 0.0;

    if (req->want_ds_ns > 0)
      cds = dabs((double)ds_ns - (double)req->want_ds_ns);

    if (req->want_speed_mps > 0)
      csp = dabs((double)sp - (double)req->want_speed_mps);

    double cost = (req->w_ds > 0 ? req->w_ds : 1.0) * cds + (req->w_speed > 0 ? req->w_speed : 0.2) * csp;

    if (!found || cost < best_cost) {
      best_cost = cost;
      found = true;

      best.model_id = model_id;
      best.n_tx = n_tx;
      best.n_rx = n_rx;
      best.L = e["L"].as<int>(0);
      best.S = e["S"].as<int>(0);
      best.fs_hz = e["fs_hz"].as<double>(0.0);
      best.snapshot_dt_s = e["snapshot_dt_s"].as<double>(0.0);
      best.ds_ns = ds_ns;
      best.speed_mps = sp;
      best.pair_order = pair_order;
      best.offset_bytes = e["offset_bytes"].as<uint64_t>(0);
      best.nbytes = e["nbytes"].as<uint64_t>(0);

      best.num_events = 0;
      auto events_node = e["events"];
      if (events_node && events_node.IsSequence()) {
        int ne = static_cast<int>(events_node.size());
        if (ne > MAX_CIRDB_EVENTS) {
          LOG_W(HW, "CIRDB YAML: entry has %d events, truncating to MAX_CIRDB_EVENTS=%d\n", ne, MAX_CIRDB_EVENTS);
          ne = MAX_CIRDB_EVENTS;
        }
        for (int ei = 0; ei < ne; ei++) {
          auto ev = events_node[ei];
          for (int j = 0; j < 6 && j < (int)ev.size(); j++)
            best.events[ei][j] = ev[j].as<double>(0.0);
        }
        best.num_events = ne;
      }
    }
  }

  if (!found)
    return 0;
  *out = best;
  return 1;
}
