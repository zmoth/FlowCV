//
// Plugin UdpSend
//

#include "UDP_Send.hpp"

using namespace DSPatch;
using namespace DSPatchables;

int32_t global_inst_counter = 0;

namespace DSPatch::DSPatchables::internal
{
class UdpSend
{
};
}  // namespace DSPatch::DSPatchables::internal

UdpSend::UdpSend() : Component(ProcessOrder::OutOfOrder), p(new internal::UdpSend())
{
    // Name and Category
    SetComponentName_("UDP_Send");
    SetComponentCategory_(Category::Category_Output);
    SetComponentAuthor_("Richard");
    SetComponentVersion_("0.1.0");
    SetInstanceCount(global_inst_counter);
    global_inst_counter++;

    // 1 inputs
    SetInputCount_(5, {"bool", "int", "float", "str", "json"},
        {IoType::Io_Type_Bool, IoType::Io_Type_Int, IoType::Io_Type_Float, IoType::Io_Type_String, IoType::Io_Type_JSON});

    // 0 outputs
    SetOutputCount_(0);

    port_ = 0;
    data_pack_mode_ = 3;
    is_multicast_ = false;
    is_valid_ip = false;
    send_as_binary_ = true;
    transmit_rate_ = 0;
    eol_seq_index_ = 0;
    current_time_ = std::chrono::steady_clock::now();
    last_time_ = current_time_;

    SetEnabled(true);
}

void HandleNetworkWrite(const std::error_code &error, std::size_t bytes_transferred)
{
    std::cout << "Wrote " << bytes_transferred << " bytes with " << error.message() << std::endl;
}

void UdpSend::SetEOLSeq_()
{
    memset(eol_seq_, '\0', 3);

    switch (eol_seq_index_) {
        case 1:
            eol_seq_[0] = '\r';
            break;
        case 2:
            eol_seq_[0] = '\n';
            break;
        case 3:
            eol_seq_[0] = '\r';
            eol_seq_[1] = '\n';
            break;
        case 4:
            eol_seq_[0] = ' ';
            break;
        case 5:
            eol_seq_[0] = '\t';
            break;
        case 6:
            eol_seq_[0] = ',';
            break;
        default:
            eol_seq_[0] = '\0';
            break;
    }
}

template<typename T> std::vector<uint8_t> UdpSend::GenerateOutBuffer_(T data)
{
    std::vector<uint8_t> outBuffer;
    if (send_as_binary_) {
        auto *d = reinterpret_cast<uint8_t *>(&data);
        for (int i = 0; i < sizeof(T); i++) {
            outBuffer.emplace_back(d[i]);
        }
    }
    else {
        std::string buf;
        if (typeid(T).name() == typeid(char).name())
            buf = data;
        else
            buf = std::to_string(data);
        for (char &i : buf) {
            outBuffer.emplace_back(i);
        }
    }
    for (int i = 0; i < 2; i++) {
        if (eol_seq_[i] != '\0')
            outBuffer.emplace_back(eol_seq_[i]);
    }
    return outBuffer;
}

void UdpSend::Process_(SignalBus const &inputs, SignalBus &outputs)
{
    // Input Handler
    auto in1 = inputs.GetValue<bool>(0);
    auto in2 = inputs.GetValue<int>(1);
    auto in3 = inputs.GetValue<float>(2);
    auto in4 = inputs.GetValue<std::string>(3);
    auto in5 = inputs.GetValue<nlohmann::json>(4);

    if (IsEnabled()) {
        if (socket_ != nullptr) {
            if (socket_->is_open()) {
                current_time_ = std::chrono::steady_clock::now();
                auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(current_time_ - last_time_).count();
                bool readyToSend = false;
                if (transmit_rate_ > 0) {
                    if (delta >= (long long)rate_val_) {
                        readyToSend = true;
                    }
                }
                else
                    readyToSend = true;

                if (in1) {
                    if (readyToSend) {
                        std::vector<uint8_t> buf = GenerateOutBuffer_<bool>(*in1);
                        socket_->async_send_to(asio::buffer(buf.data(), buf.size()), *remote_endpoint_, HandleNetworkWrite);
                    }
                }
                else if (in2) {
                    if (readyToSend) {
                        std::vector<uint8_t> buf = GenerateOutBuffer_<int>(*in2);
                        socket_->async_send_to(asio::buffer(buf.data(), buf.size()), *remote_endpoint_, HandleNetworkWrite);
                    }
                }
                else if (in3) {
                    if (readyToSend) {
                        std::vector<uint8_t> buf = GenerateOutBuffer_<float>(*in3);
                        socket_->async_send_to(asio::buffer(buf.data(), buf.size()), *remote_endpoint_, HandleNetworkWrite);
                    }
                }
                else if (in4) {
                    if (readyToSend) {
                        std::vector<uint8_t> buf = GenerateOutBuffer_<char>(*in4->c_str());
                        socket_->async_send_to(asio::buffer(buf.data(), buf.size()), *remote_endpoint_, HandleNetworkWrite);
                    }
                }
                else if (in5) {
                    if (!in5->empty()) {
                        nlohmann::json json_in_ = *in5;
                        if (readyToSend) {
                            switch (data_pack_mode_) {
                                case 1:  // BSON
                                {
                                    std::vector<uint8_t> msg = nlohmann::json::to_bson(json_in_);
                                    socket_->async_send_to(asio::buffer(msg.data(), msg.size()), *remote_endpoint_, HandleNetworkWrite);
                                    break;
                                }
                                case 2:  // CBOR
                                {
                                    std::vector<uint8_t> msg = nlohmann::json::to_cbor(json_in_);
                                    socket_->async_send_to(asio::buffer(msg.data(), msg.size()), *remote_endpoint_, HandleNetworkWrite);
                                    break;
                                }
                                case 3:  // MessagePack
                                {
                                    std::vector<uint8_t> msg = nlohmann::json::to_msgpack(json_in_);
                                    socket_->async_send_to(asio::buffer(msg.data(), msg.size()), *remote_endpoint_, HandleNetworkWrite);
                                    break;
                                }
                                case 4:  // UBJSON
                                {
                                    std::vector<uint8_t> msg = nlohmann::json::to_ubjson(json_in_);
                                    socket_->async_send_to(asio::buffer(msg.data(), msg.size()), *remote_endpoint_, HandleNetworkWrite);
                                    break;
                                }
                                default:  // None (0)
                                    std::string jsonStr = json_in_.dump();
                                    for (int i = 0; i < 2; i++) {
                                        if (eol_seq_[i] != '\0')
                                            jsonStr += eol_seq_[i];
                                    }
                                    socket_->send_to(asio::buffer(jsonStr), *remote_endpoint_, 0);
                            }
                        }
                    }
                }
                if (readyToSend) {
                    float curRate = 1.0f / ((float)delta * 0.001f);
                    // std::cout << curRate << std::endl;
                    last_time_ = current_time_;
                }
            }
        }
    }
}

bool UdpSend::HasGui(int interface)
{
    // When Creating Strings for Controls use: CreateControlString("Text Here", GetInstanceCount()).c_str()
    // This will ensure a unique control name for ImGui with multiple instance of the Plugin
    if (interface == (int)FlowCV::GuiInterfaceType_Controls) {
        return true;
    }

    return false;
}

void UdpSend::UpdateGui(void *context, int interface)
{
    auto *imCurContext = (ImGuiContext *)context;
    ImGui::SetCurrentContext(imCurContext);

    if (interface == (int)FlowCV::GuiInterfaceType_Controls) {
        bool multicastIndicator = is_multicast_;
        ImGui::Checkbox(CreateControlString("Is Multicast", GetInstanceName()).c_str(), &multicastIndicator);
        if (!is_valid_ip)
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(128, 0, 0, 255));
        else
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(29, 47, 73, 255));
        ImGui::SetNextItemWidth(140);
        if (ImGui::InputText(CreateControlString("IP Address", GetInstanceName()).c_str(), tmp_ip_buf, 64)) {
            ip_addr_ = tmp_ip_buf;
            if (IsValidIP_() && port_ != 0) {
                OpenSocket_();
            }
        }
        ImGui::PopStyleColor();
        ImGui::SetNextItemWidth(120);
        if (ImGui::InputInt(CreateControlString("Port", GetInstanceName()).c_str(), &port_)) {
            if (IsValidIP_() && port_ != 0) {
                OpenSocket_();
            }
        }
        ImGui::Separator();
        ImGui::SetNextItemWidth(120);
        ImGui::Combo(CreateControlString("JSON Data Mode", GetInstanceName()).c_str(), &data_pack_mode_, "Text\0BSON\0CBOR\0MessagePack\0UBJSON\0\0");
        ImGui::Separator();
        ImGui::SetNextItemWidth(120);
        if (ImGui::Combo(CreateControlString("Rate (Hz)", GetInstanceName()).c_str(), &transmit_rate_, "Fastest\0 60\0 30\0 20\0 15\0 10\0 5\0 2\0 1\0\0")) {
            if (transmit_rate_ > 0)
                rate_val_ = (1.0f / (float)rate_selection_[transmit_rate_]) * 1000.0f;
            else
                rate_val_ = 0;
        }
        ImGui::Separator();
        if (ImGui::Combo(
                CreateControlString("EOL Sequence", GetInstanceName()).c_str(), &eol_seq_index_, "None\0<CR>\0<LF>\0<CR><LF>\0<SPACE>\0<TAB>\0<COMMA>\0\0")) {
            SetEOLSeq_();
        }
        ImGui::Separator();
        ImGui::TextUnformatted("Non-JSON Data Type Sending Options");
        ImGui::Checkbox(CreateControlString("Send As Binary", GetInstanceName()).c_str(), &send_as_binary_);
    }
}

std::string UdpSend::GetState()
{
    using namespace nlohmann;

    json state;

    state["ip_addr"] = ip_addr_;
    state["port"] = port_;
    state["data_mode"] = data_pack_mode_;
    state["data_rate"] = transmit_rate_;
    state["eol_seq"] = eol_seq_index_;
    state["send_binary"] = send_as_binary_;

    std::string stateSerialized = state.dump(4);

    return stateSerialized;
}

void UdpSend::SetState(std::string &&json_serialized)
{
    using namespace nlohmann;

    json state = json::parse(json_serialized);

    if (state.contains("ip_addr")) {
        ip_addr_ = state["ip_addr"].get<std::string>();
        strcpy(tmp_ip_buf, ip_addr_.c_str());
    }
    if (state.contains("port"))
        port_ = state["port"].get<int>();
    if (state.contains("data_mode"))
        data_pack_mode_ = state["data_mode"].get<int>();
    if (state.contains("data_rate")) {
        transmit_rate_ = state["data_rate"].get<int>();
        if (transmit_rate_ > 0)
            rate_val_ = (1.0f / (float)rate_selection_[transmit_rate_]) * 1000.0f;
        else
            rate_val_ = 0;
    }
    if (state.contains("send_binary"))
        send_as_binary_ = state["send_binary"].get<bool>();
    if (state.contains("eol_seq")) {
        eol_seq_index_ = state["eol_seq"].get<int>();
        SetEOLSeq_();
    }

    if (IsValidIP_() && port_ != 0)
        OpenSocket_();
}

void UdpSend::OpenSocket_()
{
    if (IsValidIP_()) {
        auto address = asio::ip::address::from_string(ip_addr_);
        if (socket_ == nullptr)
            socket_ = std::make_unique<asio::ip::udp::socket>(io_service_);
        else {
            if (socket_->is_open())
                socket_->close();
        }
        remote_endpoint_ = std::make_unique<asio::ip::udp::endpoint>(address, port_);
        is_multicast_ = false;
        if (address.is_v4()) {
            socket_->open(asio::ip::udp::v4());
            if (address.is_multicast())
                is_multicast_ = true;
        }
        else if (address.is_v6()) {
            socket_->open(asio::ip::udp::v6());
            if (address.is_multicast())
                is_multicast_ = true;
        }
    }
}

void UdpSend::CloseSocket_()
{
    if (socket_->is_open())
        socket_->close();
}

bool UdpSend::IsValidIP_()
{
    if (!ip_addr_.empty()) {
        std::error_code ec;
        auto address = asio::ip::address::from_string(ip_addr_, ec);
        if (ec) {
            is_valid_ip = false;
            std::cerr << ec.message() << std::endl;
        }
        else {
            if (address.is_v4()) {
                int dotCount = 0;
                for (auto &c : ip_addr_) {
                    if (c == '.')
                        dotCount++;
                }
                if (dotCount == 3) {
                    is_valid_ip = true;
                    return true;
                }
            }
            else if (address.is_v6()) {
                int dotCount = 0;
                for (auto c : ip_addr_) {
                    if (c == ':')
                        dotCount++;
                }
                if (dotCount == 7) {
                    is_valid_ip = true;
                    return true;
                }
            }
        }
    }

    is_valid_ip = false;
    return false;
}
