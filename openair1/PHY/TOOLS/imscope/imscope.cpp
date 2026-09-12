/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include "implot.h"
#include "openair1/PHY/defs_nr_UE.h"
extern "C" {
#include "openair1/PHY/TOOLS/phy_scope_interface.h"
#include "executables/softmodem-common.h"
}
#include <iostream>
#include <vector>
#include <limits>
#include <algorithm>
#include <sstream>
#include <mutex>
#include <thread>
#include <fstream>
#include "imscope_internal.h"
#include <cstdlib>
#include <vector>

#define NR_MAX_RB 273
#define N_SC_PER_RB NR_NB_SC_PER_RB

static std::vector<int> rb_boundaries;

void copyDataThreadSafe(void *scopeData,
                        enum scopeDataType type,
                        void *dataIn,
                        int elementSz,
                        int colSz,
                        int lineSz,
                        int offset,
                        metadata *meta);
bool tryLockScopeData(enum scopeDataType type, int elementSz, int colSz, int lineSz, metadata *meta);
void copyDataUnsafeWithOffset(enum scopeDataType type, void *dataIn, size_t size, size_t offset, int copy_index);
void unlockScopeData(enum scopeDataType type);

static void glfw_error_callback(int error, const char *description)
{
  fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

MovingAverageTimer iq_procedure_timer;

ImScopeDataWrapper scope_array[EXTRA_SCOPE_TYPES];

class LLRPlot {
  int len = 0;
  float timestamp = 0;
  std::vector<int16_t> llr;
  bool frozen = false;
  bool next = false;
  metadata meta;

 public:
  void Draw(float time, enum scopeDataType type, const char *label)
  {
    ImGui::BeginGroup();
    if (ImGui::Button(frozen ? "Unfreeze" : "Freeze")) {
      frozen = !frozen;
      next = false;
    }
    if (frozen) {
      ImGui::SameLine();
      ImGui::BeginDisabled(next);
      if (ImGui::Button("Load next histogram")) {
        next = true;
      }
      ImGui::EndDisabled();
    }

    ImScopeDataWrapper &scope_data = scope_array[type];
    if (ImPlot::BeginPlot(label)) {
      ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
      if (!frozen || next) {
        if (scope_data.is_data_ready) {
          iq_procedure_timer.Add(scope_data.data.time_taken_in_ns);
          timestamp = time;
          const int16_t *tmp = (int16_t *)(scope_data.data.scope_graph_data + 1);
          len = scope_data.data.scope_graph_data->lineSz;
          llr.reserve(len);
          for (auto i = 0; i < len; i++) {
            llr[i] = tmp[i];
          }
          meta = scope_data.data.meta;
          scope_data.is_data_ready = false;
          if (frozen) {
            next = false;
          }
        }
      }

      ImPlot::PlotLine(label, llr.data(), len);
      ImPlot::EndPlot();
    }
    std::stringstream ss;
    if (meta.slot != -1) {
      ss << " slot: " << meta.slot;
    }
    if (meta.frame != -1) {
      ss << " frame: " << meta.frame;
    }
    if (!ss.str().empty()) {
      ImGui::Text("Data for %s", ss.str().c_str());
    }
    ImGui::Text("Data is %.2f seconds old", time - timestamp);
    ImGui::EndGroup();
  }
};

class IQHist {
 private:
  bool frozen = false;
  bool next = false;
  float range = 100;
  int num_bins = 33;
  std::string label;
  float min_nonzero_percentage = 0.9;
  float epsilon = 0.0;
  bool auto_adjust_range = true;
  int plot_type = 0;
  bool disable_scatterplot;

 public:
  IQHist(const char *label_, bool _disable_scatterplot = false)
  {
    label = label_;
    disable_scatterplot = _disable_scatterplot;
  };
  bool ShouldReadData(void)
  {
    return !frozen || next;
  }
  float GetEpsilon(void)
  {
    return epsilon;
  }
  void Draw(IQData *iq_data, float time, bool new_data)
  {
    if (new_data && frozen && next) {
      // Evaluate if new data matches filter settings
      if (((float)iq_data->nonzero_count / (float)iq_data->len) > min_nonzero_percentage) {
        next = false;
      }
    }
    ImGui::BeginGroup();
    ImGui::Checkbox("auto adjust range", &auto_adjust_range);
    if (auto_adjust_range) {
      if (range < iq_data->max_iq * 1.1) {
        range = iq_data->max_iq * 1.1;
      }
    }
    ImGui::BeginDisabled(auto_adjust_range);
    ImGui::SameLine();
    ImGui::DragFloat("Range", &range, 1, 0.1, std::numeric_limits<int16_t>::max());
    ImGui::EndDisabled();

    ImGui::DragInt("Number of bins", &num_bins, 1, 33, 101);
    if (ImGui::Button(frozen ? "Unfreeze" : "Freeze")) {
      frozen = !frozen;
      next = false;
    }

    if (frozen) {
      ImGui::SameLine();
      ImGui::BeginDisabled(next);
      if (ImGui::Button("Load next histogram")) {
        next = true;
      }
      ImGui::EndDisabled();
      ImGui::Text("Filter parameters");
      ImGui::DragFloat("%% nonzero elements", &min_nonzero_percentage, 1, 0.0, 100);
      ImGui::DragFloat("epsilon", &epsilon, 1, 0.0, 3000);
    }
    const char *items[] = {"Histogram", "RMS", "Scatter"};
    ImGui::Combo("Select plot type", &plot_type, items, disable_scatterplot ? 2 : 3);
    if (plot_type == 0) {
      float x = ImGui::CalcItemWidth();
      if (ImPlot::BeginPlot(label.c_str(), {x, x})) {
        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
        ImPlot::PlotHistogram2D(label.c_str(),
                                iq_data->real.data(),
                                iq_data->imag.data(),
                                iq_data->len,
                                num_bins,
                                num_bins,
                                ImPlotRect(-range, range, -range, range));
        ImPlot::EndPlot();
      }
    } else if (plot_type == 2) {
      float x = ImGui::CalcItemWidth();
      if (ImPlot::BeginPlot(label.c_str(), {x, x})) {
        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
        int points_drawn = 0;
        while (points_drawn < iq_data->len) {
          // Limit the amount of data plotted with PlotScatter call (issue with vertices/draw call)
          int points_to_draw = std::min(iq_data->len - points_drawn, 16000);
          ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 1, IMPLOT_AUTO_COL, 1);
          ImPlot::PlotScatter(label.c_str(),
                              iq_data->real.data() + points_drawn,
                              iq_data->imag.data() + points_drawn,
                              points_to_draw);
          points_drawn += points_to_draw;
        }
        ImPlot::EndPlot();
      }
    } else if (plot_type == 1) {
      if (ImPlot::BeginPlot(label.c_str())) {
        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
        ImPlot::PlotLine(label.c_str(), iq_data->power.data(), iq_data->len);
        ImPlot::EndPlot();
      }
    }
    ImGui::Text("Maximum value = %d, nonzero elements/total %d/%d", iq_data->max_iq, iq_data->nonzero_count, iq_data->len);
    ImGui::Text("Data is %.2f seconds old", time - iq_data->timestamp);
    std::stringstream ss;
    if (iq_data->meta.slot != -1) {
      ss << " slot: " << iq_data->meta.slot;
    }
    if (iq_data->meta.frame != -1) {
      ss << " frame: " << iq_data->meta.frame;
    }
    if (!ss.str().empty()) {
      ImGui::Text("Data for %s", ss.str().c_str());
    }
    if (ImGui::Button("Save IQ")) {
      std::stringstream ss;
      ss << "iq_";
      if (iq_data->meta.frame != -1) {
        ss << iq_data->meta.frame << "_";
      }
      if (iq_data->meta.slot != -1) {
        ss << iq_data->meta.slot << "_";
      }
      ss << scope_id_to_string(static_cast<enum scopeDataType>(iq_data->scope_id));
      ss << ".csv";
      std::ofstream file(ss.str());
      if (file.is_open()) {
        file << "real;imag\n";
        for (int i = 0; i < iq_data->len; i++) {
          file << iq_data->real[i] << ";" << iq_data->imag[i] << "\n";
        }
        file.close();
        std::cout << "Saved IQ to file: " << ss.str() << std::endl;
      } else {
        std::cerr << "Unable to open file";
      }
    }
    ImGui::EndGroup();
  }
};

class IQSlotHeatmap {
 private:
  bool frozen = false;
  bool next = false;
  float timestamp = 0;
  std::vector<float> power;
  ImScopeDataWrapper *scope_data;
  std::string label;
  int len = 0;
  float max = 0;
  float stop_at_min = 1000;

 public:
  IQSlotHeatmap(ImScopeDataWrapper *scope_data_, const char *label_)
  {
    scope_data = scope_data_;
    label = label_;
  };
  /* The frequency domain buffers are FFT shifted: the first negative frequency of the
     carrier is at index 0 and the REs of the whole carrier are contiguous. */
  void ReadData(float time, int ofdm_symbol_size, int num_symbols, int num_rb)
  {
    auto num_sc = num_rb * NR_NB_SC_PER_RB;
    if (!frozen || next) {
      if (scope_data->is_data_ready) {
        iq_procedure_timer.Add(scope_data->data.time_taken_in_ns);
        timestamp = time;
        scopeGraphData_t *iq_header = scope_data->data.scope_graph_data;
        len = iq_header->lineSz;
        c16_t *source = (c16_t *)(iq_header + 1);

        power.reserve(num_sc * num_symbols);
        for (auto symbol = 0; symbol < num_symbols; symbol++) {
          for (auto sc = 0; sc < num_sc; sc++) {
            auto source_index = sc + symbol * ofdm_symbol_size;
            power[sc * num_symbols + symbol] = std::pow(source[source_index].r, 2) + std::pow(source[source_index].i, 2);
          }
        }
        max = *std::max_element(power.begin(), power.end());
        if (frozen && max > stop_at_min) {
          next = false;
        }
        scope_data->is_data_ready = false;
      }
    }
  }
  void Draw(float time, int ofdm_symbol_size, int num_symbols, int num_rb)
  {
    ReadData(time, ofdm_symbol_size, num_symbols, num_rb);
    ImGui::BeginGroup();
    if (ImGui::Button(frozen ? "Unfreeze" : "Freeze")) {
      frozen = !frozen;
      next = false;
    }
    if (frozen) {
      ImGui::SameLine();
      ImGui::BeginDisabled(next);
      if (ImGui::Button("Load next data")) {
        next = true;
      }
      ImGui::EndDisabled();
      ImGui::Text("Filter parameters:");
      ImGui::InputFloat("Max Power minimum", &stop_at_min, 10, 100);
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Data with maximum power below that value will be discarded.");
      }
    }
    static std::vector<int> symbol_boundaries = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
    if (ImPlot::BeginPlot(label.c_str(), {(float)ImGui::GetWindowWidth() * 0.9f, 0})) {
      auto num_sc = num_rb * NR_NB_SC_PER_RB;
      ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
      ImPlot::SetupAxes("symbols", "subcarriers");
      ImPlot::SetupAxisLimits(ImAxis_X1, num_symbols, 0);
      ImPlot::SetupAxisLimits(ImAxis_Y1, num_sc, 0);
      ImPlot::PlotHeatmap(label.c_str(),
                          power.data(),
                          num_sc,
                          num_symbols,
                          0,
                          max,
                          nullptr,
                          {0, 0},
                          {(double)num_symbols, (double)num_sc});
      ImPlot::PlotInfLines("Symbol boundary", symbol_boundaries.data(), symbol_boundaries.size());
      ImPlot::PlotInfLines("RB boundary", rb_boundaries.data(), num_rb, ImPlotInfLinesFlags_Horizontal);
      ImPlot::EndPlot();
    }
    ImGui::SameLine();
    ImPlot::ColormapScale("##HeatScale", 0, max);
    ImGui::Text("Data is %.2f seconds old", time - timestamp);
    ImGui::EndGroup();
  }
};

// utility structure for realtime plot
struct ScrollingBuffer {
  int MaxSize;
  int Offset;
  ImVector<ImVec2> Data;
  ScrollingBuffer(int max_size = 2000)
  {
    MaxSize = max_size;
    Offset = 0;
    Data.reserve(MaxSize);
  }
  void AddPoint(float x, float y)
  {
    if (Data.size() < MaxSize)
      Data.push_back(ImVec2(x, y));
    else {
      Data[Offset] = ImVec2(x, y);
      Offset = (Offset + 1) % MaxSize;
    }
  }
  void Erase()
  {
    if (Data.size() > 0) {
      Data.shrink(0);
      Offset = 0;
    }
  }
};

void ShowUeScope(void *data_void_ptr, float t)
{
  PHY_VARS_NR_UE *ue = (PHY_VARS_NR_UE *)data_void_ptr;
  ImGui::Begin("UE KPI");
  if (ImPlot::BeginPlot("##Scrolling", ImVec2(-1, 150))) {
    static float history = 10.0f;
    ImGui::SliderFloat("History", &history, 1, 30, "%.1f s");
    static ScrollingBuffer rbs_buffer;
    static ScrollingBuffer bler;
    static ScrollingBuffer mcs;
    rbs_buffer.AddPoint(t, getKPIUE()->nofRBs);
    bler.AddPoint(t, (float)getKPIUE()->nb_nack / (float)getKPIUE()->nb_total);
    mcs.AddPoint(t, (float)getKPIUE()->dl_mcs);
    ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
    ImPlot::SetupAxes("time", "noOfRbs");
    ImPlot::SetupAxisLimits(ImAxis_X1, t - history, t, ImGuiCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, 0, NR_MAX_RB);
    ImPlot::SetupAxis(ImAxis_Y2, "bler [%]", ImPlotAxisFlags_AuxDefault);
    ImPlot::SetupAxis(ImAxis_Y3, "MCS", ImPlotAxisFlags_AuxDefault);
    ImPlot::SetAxes(ImAxis_X1, ImAxis_Y1);
    ImPlot::PlotLine("noOfRbs", &rbs_buffer.Data[0].x, &rbs_buffer.Data[0].y, rbs_buffer.Data.size(), 0, 0, 2 * sizeof(float));
    ImPlot::SetAxes(ImAxis_X1, ImAxis_Y2);
    ImPlot::PlotLine("bler", &bler.Data[0].x, &bler.Data[0].y, bler.Data.size(), 0, 0, 2 * sizeof(float));
    ImPlot::SetAxes(ImAxis_X1, ImAxis_Y3);
    ImPlot::PlotLine("mcs", &mcs.Data[0].x, &mcs.Data[0].y, mcs.Data.size(), 0, 0, 2 * sizeof(float));
    ImPlot::EndPlot();
  }
  ImGui::End();

  if (ImGui::Begin("UE PDCCH IQ")) {
    static auto iq_data = new IQData();
    static auto pdcch_iq_hist = new IQHist("PDCCH IQ");
    bool new_data = false;
    if (pdcch_iq_hist->ShouldReadData()) {
      new_data = iq_data->TryCollect(&scope_array[pdcchRxdataF_comp], t, pdcch_iq_hist->GetEpsilon(), iq_procedure_timer);
    }
    pdcch_iq_hist->Draw(iq_data, t, new_data);
  }
  ImGui::End();

  if (ImGui::Begin("UE PDCCH LLR")) {
    static auto llr_plot = new LLRPlot();
    llr_plot->Draw(t, pdcchLlr, "PDCCH LLR");
  }
  ImGui::End();

  if (ImGui::Begin("UE PDSCH IQ")) {
    static auto iq_data = new IQData();
    static auto pdsch_iq_hist = new IQHist("PDSCH IQ");
    bool new_data = false;
    if (pdsch_iq_hist->ShouldReadData()) {
      new_data = iq_data->TryCollect(&scope_array[pdschRxdataF_comp], t, pdsch_iq_hist->GetEpsilon(), iq_procedure_timer);
    }
    pdsch_iq_hist->Draw(iq_data, t, new_data);
  }
  ImGui::End();

  if (ImGui::Begin("UE PDSCH Chan est")) {
    static auto iq_data = new IQData();
    static auto iq_hist = new IQHist("PDSCH Chan est IQ");
    bool new_data = false;
    if (iq_hist->ShouldReadData()) {
      new_data = iq_data->TryCollect(&scope_array[pdschChanEstimates], t, iq_hist->GetEpsilon(), iq_procedure_timer);
    }
    iq_hist->Draw(iq_data, t, new_data);
  }
  ImGui::End();

  if (ImGui::Begin("UE PDSCH IQ before compensation")) {
    static auto iq_data = new IQData();
    static auto iq_hist = new IQHist("PDSCH IQ before compensation");
    bool new_data = false;
    if (iq_hist->ShouldReadData()) {
      new_data = iq_data->TryCollect(&scope_array[pdschRxdataF], t, iq_hist->GetEpsilon(), iq_procedure_timer);
    }
    iq_hist->Draw(iq_data, t, new_data);
  }
  ImGui::End();


  if (ImGui::Begin("Time domain samples")) {
    static auto iq_data = new IQData();
    // Issue with imgui deferring draw calls until the end of the frame - cases segfault if scatterplot has too many points
    bool disable_scatterplot = true;
    static auto time_domain_iq = new IQHist("Time domain samples", disable_scatterplot);
    bool new_data = false;
    if (time_domain_iq->ShouldReadData()) {
      new_data = iq_data->TryCollect(&scope_array[ueTimeDomainSamples], t, time_domain_iq->GetEpsilon(), iq_procedure_timer);
    }
    time_domain_iq->Draw(iq_data, t, new_data);
  }
  ImGui::End();

  if (ImGui::Begin("Time domain samples - before sync")) {
    static auto iq_data = new IQData();
    // Issue with imgui deferring draw calls until the end of the frame - cases segfault if scatterplot has too many points
    bool disable_scatterplot = true;
    static auto time_domain_iq = new IQHist("Time domain samples - before sync", disable_scatterplot);
    bool new_data = false;
    if (time_domain_iq->ShouldReadData()) {
      new_data = iq_data->TryCollect(&scope_array[ueTimeDomainSamplesBeforeSync], t, time_domain_iq->GetEpsilon(), iq_procedure_timer);
    }
    time_domain_iq->Draw(iq_data, t, new_data);
  }
  ImGui::End();

  if (ImGui::Begin("Broadcast channel")) {
    ImGui::Text("RSRP %d", ue->measurements.ssb_rsrp_dBm[ue->frame_parms.ssb_index]);
    if (ImGui::TreeNode("IQ")) {
      static auto iq_data = new IQData();
      static auto broadcast_iq_hist = new IQHist("Broadcast IQ");
      bool new_data = false;
      if (broadcast_iq_hist->ShouldReadData()) {
        new_data = iq_data->TryCollect(&scope_array[ue->sl_mode ? psbchRxdataF_comp : pbchRxdataF_comp],
                                       t,
                                       broadcast_iq_hist->GetEpsilon(), iq_procedure_timer);
      }
      broadcast_iq_hist->Draw(iq_data, t, new_data);
      ImGui::TreePop();
    }
    if (ImGui::TreeNode("CHest")) {
      static auto chest_iq_data = new IQData();
      static auto broadcast_iq_chest = new IQHist("Broadcast Chest");
      bool new_data = false;
      if (broadcast_iq_chest->ShouldReadData()) {
        new_data = chest_iq_data->TryCollect(&scope_array[ue->sl_mode ? psbchDlChEstimateTime : pbchDlChEstimateTime],
                                             t,
                                             broadcast_iq_chest->GetEpsilon(), iq_procedure_timer);
      }
      broadcast_iq_chest->Draw(chest_iq_data, t, new_data);
      ImGui::TreePop();
    }

    if (ImGui::TreeNode("LLR")) {
      static auto llr_plot = new LLRPlot();
      llr_plot->Draw(t, ue->sl_mode ? psbchLlr : pbchLlr, "Broadcast LLR");
      ImGui::TreePop();
    }
  }
  ImGui::End();

  // if (ImGui::Begin("RX IQ")) {
  //   static auto common_rx_iq_heatmap = new IQSlotHeatmap(&scope_array[commonRxdataF], "common RX IQ");
  //   common_rx_iq_heatmap->Draw(t,
  //                              ue->frame_parms.ofdm_symbol_size,
  //                              ue->frame_parms.symbols_per_slot,
  //                              ue->frame_parms.N_RB_DL);
  // }
  // ImGui::End();
}

void ShowGnbScope(void *data_void_ptr, float t)
{
  (void)data_void_ptr;
  // if (ImGui::TreeNode("RX IQ")) {
  //   static auto gnb_heatmap = new IQSlotHeatmap(&scope_array[gNBRxdataF], "common RX IQ");

  //   gnb_heatmap->Draw(t,
  //                     gNB->frame_parms.ofdm_symbol_size,
  //                     gNB->frame_parms.symbols_per_slot,
  //                     gNB->frame_parms.N_RB_UL);
  //   ImGui::TreePop();
  // }
  if (ImGui::Begin("PUSCH SLOT IQ")) {
    static auto pusch_iq = new IQData();
    static auto pusch_iq_display = new IQHist("PUSCH compensated IQ");
    bool new_data = false;
    if (pusch_iq_display->ShouldReadData()) {
      new_data = pusch_iq->TryCollect(&scope_array[gNBPuschRxIq], t, pusch_iq_display->GetEpsilon(), iq_procedure_timer);
    }
    pusch_iq_display->Draw(pusch_iq, t, new_data);
  }
  ImGui::End();

  if (ImGui::Begin("PUSCH LLRs")) {
    static auto pusch_llr_plot = new LLRPlot();
    pusch_llr_plot->Draw(t, gNBPuschLlr, "PUSCH LLR");
  }
  ImGui::End();

  if (ImGui::Begin("Time domain samples")) {
    static auto iq_data = new IQData();
    // Issue with imgui deferring draw calls until the end of the frame - cases segfault if scatterplot has too many points
    bool disable_scatterplot = true;
    static auto time_domain_iq = new IQHist("Time domain samples", disable_scatterplot);
    bool new_data = false;
    if (time_domain_iq->ShouldReadData()) {
      new_data = iq_data->TryCollect(&scope_array[gNbTimeDomainSamples], t, time_domain_iq->GetEpsilon(), iq_procedure_timer);
    }
    time_domain_iq->Draw(iq_data, t, new_data);
  }
  ImGui::End();
}

void ShowIQFileViewer(void *data_void_ptr)
{
  auto iq_data = static_cast<std::vector<IQData> *>(data_void_ptr);
  if (ImGui::Begin("Scope selection")) {
    static int selected_scope = 0;
    ImGui::Combo(
        "Select scope",
        &selected_scope,
        [](void *userdata, int idx) {
          std::vector<IQData> *iq_data = static_cast<std::vector<IQData>*>(userdata);
          return scope_id_to_string(static_cast<scopeDataType>((*iq_data)[idx].scope_id));
        },
        iq_data,
        iq_data->size());
    static auto iq_display = new IQHist("IQ File Viewer");
    iq_display->Draw(&(*iq_data)[selected_scope], 0, false);
  }
  ImGui::End();
}

void *imscope_thread(void *data_void_ptr)
{
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit())
    return nullptr;

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
  // GL ES 2.0 + GLSL 100
  const char *glsl_version = "#version 100";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
  // GL 3.2 + GLSL 150
  const char *glsl_version = "#version 150";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Required on Mac
#else
  // GL 3.0 + GLSL 130
  const char *glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
  // glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

  // Create window with graphics context
  GLFWwindow *window = glfwCreateWindow(1280, 720, "imscope", nullptr, nullptr);
  if (window == nullptr)
    return nullptr;
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1); // For frame capping

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImPlot::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  // ImGui::StyleColorsLight();

  // Setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __EMSCRIPTEN__
  ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, "#canvas");
#endif
  ImGui_ImplOpenGL3_Init(glsl_version);

  // Our state
  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  for (auto i = 0U; i < NR_MAX_RB; i++) {
    rb_boundaries.push_back(i * NR_NB_SC_PER_RB);
  }

  static double last_frame_time = glfwGetTime();
  static int target_fps = 24;

  bool is_ue = IS_SOFTMODEM_5GUE;
  bool is_gnb = IS_SOFTMODEM_GNB;
  bool close_window = false;
  while (!glfwWindowShouldClose(window) && close_window == false) {
    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy
    // of the mouse data.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your
    // copy of the keyboard data. Generally you may always pass all inputs to dear imgui, and hide them from your application based
    // on those two flags.
    glfwPollEvents();

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    static bool reset_ini_settings = false;
    if (reset_ini_settings) {
      ImGui::LoadIniSettingsFromDisk("imscope-init.ini");
      reset_ini_settings = false;
    }
    ImGui::NewFrame();

    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);

    static float t = 0;
    static bool show_imgui_demo_window = false;
    static bool show_implot_demo_window = false;
    static bool show_scope_settings_window = false;
    ImGui::DockSpaceOverViewport();
    if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Close scope")) {
          close_window = true;
        }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Options")) {
        ImGui::Checkbox("Show imgui demo window", &show_imgui_demo_window);
        ImGui::Checkbox("Show implot demo window", &show_implot_demo_window);
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Layout")) {
        if (ImGui::MenuItem("Reset")) {
          reset_ini_settings = true;
        }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Settings")) {
        if (ImGui::MenuItem("Global scope settings")) {
          show_scope_settings_window = !show_scope_settings_window;
        }
        ImGui::EndMenu();
      }
      ImGui::EndMainMenuBar();
    }

    ImGui::Begin("Status bar");
    ImGui::Text("Total time used by IQ capture procedures per milisecond: %.2f [us]/[ms]", iq_procedure_timer.average / 1000);
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Total time used in PHY threads for copying out IQ data for the scope, in uS, averaged over 1 ms");
    }
    ImGui::End();

    t += ImGui::GetIO().DeltaTime;
    iq_procedure_timer.UpdateAverage(t);

    if (is_ue) {
      ShowUeScope(data_void_ptr, t);
    } else if (is_gnb) {
      ShowGnbScope(data_void_ptr, t);
    } else {
      ShowIQFileViewer(data_void_ptr);
    }

    // For reference
    if (show_implot_demo_window)
      ImPlot::ShowDemoWindow();
    if (show_imgui_demo_window)
      ImGui::ShowDemoWindow();
    // Settings
    if (show_scope_settings_window) {
      ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
      ImGui::Begin("Global scope settings", &show_scope_settings_window);
      ImGui::ShowStyleSelector("ImGui Style");
      ImPlot::ShowStyleSelector("ImPlot Style");
      ImPlot::ShowColormapSelector("ImPlot Colormap");
      ImGui::SliderInt("FPS target", &target_fps, 12, 60);
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Reduces scope flickering in unfrozen mode. Can reduce impact on perfromance of the modem");
      }
      ImGui::End();
    }
    // Rendering
    ImGui::Render();
    glViewport(0, 0, display_w, display_h);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    double target_frame_time = 1.0 / target_fps;
    double delta = glfwGetTime() - last_frame_time;
    if (delta < target_frame_time) {
      std::this_thread::sleep_for(std::chrono::duration<float>(target_frame_time - delta));
    }

    glfwSwapBuffers(window);
    last_frame_time = glfwGetTime();
  }

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

  return nullptr;
}
