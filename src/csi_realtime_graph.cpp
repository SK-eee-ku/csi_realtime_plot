#include <Packet.h>
#include <PayloadLayer.h>
#include <PcapLiveDeviceList.h>
#include <SystemUtils.h>
#include <UdpLayer.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <vector>

#include "csi_capture.hpp"
#include "csi_reader_func.hpp"
#include "csi_realtime_graph.hpp"

namespace csirdr {
Csi_plot::Csi_plot(std::string target_mac, int nrx, int ntx, bool new_header,
                   std::string wlan_std, std::string output_dir)
    : Csi_capture("wlan0", target_mac, nrx, ntx, new_header, wlan_std,
                  output_dir) {
  ;
}

Csi_plot::~Csi_plot() { ; }

void Csi_plot::run_graph(uint32_t time_sec, int top, int n_sub,
                         std::string graph_type) {
  this->graph_type = graph_type;
  this->save_mode = 2;
  this->label = -1;

  this->gnuplot = popen("gnuplot", "w");
  // gnuplot plot option setting
  fprintf(this->gnuplot, "set style line 1 lw 5 lc \'blue\'\n");
  fprintf(this->gnuplot, "set datafile separator \',\'\n");
  fprintf(this->gnuplot, "set terminal x11\n");
  fprintf(this->gnuplot, "set nokey\n");
  fprintf(this->gnuplot, "set xlabel font\"*,20\"\n");
  fprintf(this->gnuplot, "set ylabel font\"*,20\"\n");
  fprintf(this->gnuplot, "set tics font\"*,20\"\n");
  fprintf(this->gnuplot, "set xlabel offset 0,0\n");
  fprintf(this->gnuplot, "set ylabel offset -2,0\n");
  fprintf(this->gnuplot, "set lmargin 12\n");
  fprintf(this->gnuplot, "set xlabel \"Subcarrier index\"\n");

  if (graph_type == "abs") {
    fprintf(this->gnuplot, "set ylabel \"CSI amplitude\"\n");
    fprintf(this->gnuplot, "set xrange [0:%d]\n", n_sub);
    fprintf(this->gnuplot, "set yrange [0:%d]\n", top);
  } else if (graph_type == "arg") {
    fprintf(this->gnuplot, "set ylabel \"CSI phase\"\n");
    fprintf(this->gnuplot, "set xrange [0:%d]\n", n_sub);
    fprintf(this->gnuplot, "set yrange [-pi:pi]\n");
  }
  fflush(this->gnuplot);

  // デバイスのオープン
  if (!this->dev->open()) {
    std::cerr << "Cannot open devie: " << this->interface << std::endl;
  }

  // キャプチャ開始
  this->dev->pcpp::PcapLiveDevice::startCapture(this->on_packet_arrives_gnuplot,
                                                this);

  // 測定時間のsleep
  // この時間の処理は，キャプチャー時のコールバック関数で実装する
  pcpp::multiPlatformSleep(time_sec);

  // キャプチャ終了
  this->dev->pcpp::PcapLiveDevice::stopCapture();

  // デバイスのクローズ
  this->dev->close();

  pclose(this->gnuplot);
}

void Csi_plot::on_packet_arrives_gnuplot(pcpp::RawPacket *raw_packet,
                                         pcpp::PcapLiveDevice *dev,
                                         void *cookie) {
  pcpp::Packet parsed_packet(raw_packet);
  if (!parsed_packet.isPacketOfType(pcpp::UDP))
    return;

  csirdr::Csi_plot *cap = (csirdr::Csi_plot *)cookie;

  // デコードしてクラスのメンバ変数にCSIを一時保存
  cap->load_packet(parsed_packet);

  // MACアドレスの表示
  std::cout << "target MAC address: " << cap->get_target_mac_add()
            << ", this CSI MAC address: " << cap->get_temp_mac_add()
            << std::endl;

  // CSIの数が足りてなければ終了
  if (!cap->is_full_temp_csi()) {
    return;
  }

  // MACアドレスが異なれば終了
  if (!cap->is_target_mac()) {
    cap->clear_temp_csi();
    return;
  }

  // ビーコンフレームなら終了
  if (cap->is_from_beacon()) {
    cap->clear_temp_csi();
    return;
  }

  // mode 2 （振幅，位相）で保存
  cap->write_temp_csi(cap->csi_save_mode(), cap->csi_label(), false);

  // gnuplotで処理
  fprintf(cap->gnuplot, cap->graph_command().c_str());
  fflush(cap->gnuplot);

  cap->clear_temp_csi();
}

std::string Csi_plot::graph_command() {
  std::stringstream gnuplot_cmd_ss;
  gnuplot_cmd_ss << "plot \"" << this->path_csi_value.string() << "\"";

  if (this->graph_type == "abs") {
    gnuplot_cmd_ss << " using ($0):($1) ls 1 with lines" << std::endl;
  } else if (this->graph_type == "arg") {
    gnuplot_cmd_ss << " using ($0):($2) ls 1 with lines" << std::endl;
  }

  return gnuplot_cmd_ss.str();
}

} // namespace csirdr
