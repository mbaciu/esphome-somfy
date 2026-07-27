#pragma once
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/cover/cover.h"
#include "SomfyRts.h"
#include <Arduino.h>
#include <Preferences.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <nvs_flash.h>
#include <queue>

namespace esphome {
namespace somfy {

// cmd 0 - Prints the current rolling code
// cmd 11 - Program mode
// cmd 16 - Program mode for grail curtains
// cmd 21 - Delete rolling code file
// cmd 50 - Long Program mode
// cmd 61 - Clears all Preferences set
// cmd 90 - Re-run the setup member
// cmd 97 - Set the CC1101 module to TX mode
// cmd 98 - Set the CC1101 module to idle
// cmd 99 - Set the transmit pin to HIGH
// cmd 100 - Set the transmit pin to LOW

// REMOTE_TX_PIN is intentionally NOT redefined here (SomfyRts.h already
// defines it, guarded by #ifndef). The old unguarded #define in this file
// used to silently shadow any build-flag override -- but note it barely
// matters either way: this pin is only ever set LOW once in SomfyRts::init()
// and is never touched again. The actual TX path goes through the CC1101's
// SPI FIFO (see loop() below), not a bit-banged GPIO.
#define REMOTE_FIRST_ADDR 0x121311 // Starting number for remote indexes

// Fallback pin defaults match the original project's wiring doc (Wemos D1
// Mini / plain ESP32). Boards that don't expose these exact GPIOs (e.g. the
// Seeed XIAO ESP32-S3, which has a much smaller pinout) MUST override them
// via the top-level `somfy:` component config -- see __init__.py / cover.py.
// Previously these were never set at all, which left the CC1101 driver's
// globals at their zero-initialized default (GPIO0) on some cores, or at
// whatever the driver's per-architecture #ifdef block guessed for a bare
// "ESP32" target -- neither of which accounts for board-specific pinouts.
#ifndef SOMFY_DEFAULT_SCK_PIN
#define SOMFY_DEFAULT_SCK_PIN 18
#endif
#ifndef SOMFY_DEFAULT_MISO_PIN
#define SOMFY_DEFAULT_MISO_PIN 19
#endif
#ifndef SOMFY_DEFAULT_MOSI_PIN
#define SOMFY_DEFAULT_MOSI_PIN 23
#endif
#ifndef SOMFY_DEFAULT_CSN_PIN
#define SOMFY_DEFAULT_CSN_PIN 5
#endif
#ifndef SOMFY_DEFAULT_GDO0_PIN
#define SOMFY_DEFAULT_GDO0_PIN 2
#endif
#ifndef SOMFY_DEFAULT_GDO2_PIN
#define SOMFY_DEFAULT_GDO2_PIN 4
#endif

class SomfyCover : public Component, public cover::Cover
{

private:
    int index;
    int remoteId = -1;
    bool invert_ = false;
    unsigned char _buffer[64];

    SomfyRts* rtsDevice;

public:
    static ELECHOUSE_CC1101 cc1101;
    static std::queue<unsigned char> _bufferQueue;

    // Radio pins, shared across every `cover: - platform: somfy` instance
    // since they all talk to the same physical CC1101. Set once from the
    // top-level `somfy:` component (see __init__.py) before any cover's
    // setup() runs; the SOMFY_DEFAULT_* fallbacks above cover the case where
    // no `somfy:` block is present at all.
    static int sck_pin;
    static int miso_pin;
    static int mosi_pin;
    static int csn_pin;
    static int gdo0_pin;
    static int gdo2_pin;

    static void configureRadioPins(int sck, int miso, int mosi, int csn, int gdo0, int gdo2)
    {
        sck_pin = sck;
        miso_pin = miso;
        mosi_pin = mosi;
        csn_pin = csn;
        gdo0_pin = gdo0;
        gdo2_pin = gdo2;
    }

    void setCoverID(int coverID)
    {
        index = coverID;
        remoteId = REMOTE_FIRST_ADDR + coverID;
    }

    void setInvert(bool invert)
    {
        invert_ = invert;
    }

    void setup() override
    {
        rtsDevice = new SomfyRts(remoteId, &_bufferQueue);

        ESP_LOGD("SomfyCover.h", "Cover %d", index);

        // This will be called by App.setup()
        ESP_LOGD("SomfyCover.h", "Starting Device");
        Serial.begin(115200);
        Serial.println("Initialize remote device");
        ESP_LOGD("SomfyCover.h", "Somfy ESPHome Cover v1.00");
        ESP_LOGD("SomfyCover.h", "Initialize remote device");

        // Open the preference memory to create the space if necessary
        ::Preferences preferences;
        preferences.begin("SomfyCover", false);
        preferences.end();

        rtsDevice->init();

        // Must happen before cc1101.Init() -- the driver's SPI.begin() and
        // GDO pinMode() calls read these globals at Init() time. This is the
        // actual fix for board-specific wiring (see class members above):
        // previously these were never called, so the driver always used
        // either GPIO0 (uninitialized globals) or its Wemos-D1/plain-ESP32
        // #ifdef default, regardless of what's actually wired on this board.
        ESP_LOGD("SomfyCover.h", "Configuring CC1101 pins: SCK=%d MISO=%d MOSI=%d CSN=%d GDO0=%d GDO2=%d",
                 sck_pin, miso_pin, mosi_pin, csn_pin, gdo0_pin, gdo2_pin);
        cc1101.setSpiPin(sck_pin, miso_pin, mosi_pin, csn_pin);
        cc1101.setGDO(gdo0_pin, gdo2_pin);

        if (cc1101.getCC1101()) {
            ESP_LOGD("SomfyCover.h", "Communication established with the CC1101 module");
        }
        else {
            ESP_LOGD("SomfyCover.h", "Error: Could not establish communication with the CC1101 module");
        }

        cc1101.Init();
        
        cc1101.setCCMode(1);
        cc1101.setMHZ(433.42);
        cc1101.setModulation(2); // ASK/OOK
        cc1101.setDRate(1.557);  // Set the Data Rate in kBaud. Value from 0.02 to 1621.83.
        cc1101.setCrc(0);
        cc1101.setSyncMode(0); 

        cc1101.setChannel(0);         // Set the Channelnumber from 0 to 255. Default is cahnnel 0.

        //cc1101.setPA(10);             // Set TxPower. The following settings are possible depending on the frequency band.  (-30  -20  -15  -10  -6    0    5    7    10   11   12) Default is max!
        cc1101.setSyncWord(0, 0); // Set sync word. Must be the same for the transmitter and receiver. (Syncword high, Syncword low)
        cc1101.setAdrChk(0);          // Controls address check configuration of received packages. 0 = No address check. 1 = Address check, no broadcast. 2 = Address check and 0 (0x00) broadcast. 3 = Address check and 0 (0x00) and 255 (0xFF) broadcast.
        cc1101.setAddr(0);            // Address used for packet filtration. Optional broadcast addresses are 0 (0x00) and 255 (0xFF).
        cc1101.setWhiteData(0);       // Turn data whitening on / off. 0 = Whitening off. 1 = Whitening on.
        cc1101.setPktFormat(0);       // Format of RX and TX data. 0 = Normal mode, use FIFOs for RX and TX. 1 = Synchronous serial mode, Data in on GDO0 and data out on either of the GDOx pins. 2 = Random TX mode; sends random data using PN9 generator. Used for test. Works as normal mode, setting 0 (00), in RX. 3 = Asynchronous serial mode, Data in on GDO0 and data out on either of the GDOx pins.
        cc1101.setLengthConfig(2);    // 0 = Fixed packet length mode. 1 = Variable packet length mode. 2 = Infinite packet length mode. 3 = Reserved
        cc1101.setPacketLength(0);    // Indicates the packet length when fixed packet length mode is enabled. If variable packet length mode is used, this value indicates the maximum packet length allowed.

        cc1101.setManchester(0);      // Enables Manchester encoding/decoding. 0 = Disable. 1 = Enable.
        cc1101.setFEC(0);             // Enable Forward Error Correction (FEC) with interleaving for packet payload (Only supported for fixed packet length mode. 0 = Disable. 1 = Enable.
        cc1101.setPRE(0);             // Sets the minimum number of preamble bytes to be transmitted. Values: 0 : 2, 1 : 3, 2 : 4, 3 : 6, 4 : 8, 5 : 12, 6 : 16, 7 : 24
        cc1101.setPQT(0);             // Preamble quality estimator threshold. The preamble quality estimator increases an internal counter by one each time a bit is received that is different from the previous bit, and decreases the counter by 8 each time a bit is received that is the same as the last bit. A threshold of 4∙PQT for this counter is used to gate sync word detection. When PQT=0 a sync word is always accepted.
        cc1101.setAppendStatus(0);    // When enabled, two status bytes will be appended to the payload of the packet. The status bytes contain RSSI and LQI values, as well as CRC OK.


        cc1101.SpiStrobe(CC1101_SIDLE);
        // Flush TX FIFO
        cc1101.SpiStrobe(CC1101_SFTX);
    }

    void loop() override
    {
        // Get the number of free bytes in the FIFO and status of the chip
        byte status = cc1101.SpiStrobe(CC1101_SNOP | 0x40);
        
        // See the state
        byte state = status >> 4 & 0x7;

        if(state == 2 && (status & 0x0F) < 10 && _bufferQueue.size() > (status & 0x0F)) {
            // Everything going well, wait for enough room in the FIFO
            delay(1);
            return;
        }

        if (_bufferQueue.size() > 0) {
            ESP_LOGD("SomfyCover.h", "Status: 0x%02X, bytes left: %d", status, _bufferQueue.size());

            int bytesToWrite = _min(_bufferQueue.size(), status & 0x0F);

            // `==` binds tighter than `&`, so the previous
            // `status & 0x0F == 0x0F` actually evaluated as
            // `status & (0x0F == 0x0F)` = `status & 1`, not the intended
            // "are the low 4 bits all set" check.
            if ((status & 0x0F) == 0x0F && state == 0) {
                // Start of transmission, TX FIFO is supposed to be empty transmit 64 bytes
                bytesToWrite = _min(_bufferQueue.size(), 64);
            }

            if (bytesToWrite == 0) {
                if (state == 4) {
                    // Chip is calibrating
                    ESP_LOGD("SomfyCover.h", "Calibration in progress");
                    delayMicroseconds(500);
                } else if (state != 2) {
                    // Something very wrong happened, flush everything
                    ESP_LOGD("SomfyCover.h", "Something wrong occured, CC1101 not in transmit but FIFO full, flush FIFO");
                    cc1101.SpiStrobe(CC1101_SIDLE);
                    cc1101.SpiStrobe(CC1101_SFTX);
                }

                // Nothing to write
                return;
            }

            // Add bits to the FIFO
            for (int i = 0; i < bytesToWrite; i++) {
                _buffer[i] = _bufferQueue.front();
                _bufferQueue.pop();
            }
            byte resp = cc1101.SpiWriteBurstReg(CC1101_TXFIFO, _buffer, bytesToWrite);
            ESP_LOGD("SomfyCover.h", "Wrote %d bytes to FIFO, resp 0x%02X", bytesToWrite, resp);

            if (state == 7) {
                // TX Underflow
                ESP_LOGD("SomfyCover.h", "TX Underflow occured");
            }

            if (state != 2) {
                ESP_LOGD("SomfyCover.h", "Switch to transmit");
                cc1101.SpiStrobe(CC1101_SIDLE);
                cc1101.SpiStrobe(CC1101_STX); //start send
            }
        } else if(state == 7) {
            // Done writing, switch to IDLE
            ESP_LOGD("SomfyCover.h", "Switch to IDLE");
            cc1101.SpiStrobe(CC1101_SIDLE); // Exit RX / TX, turn off frequency synthesizer and exit
            cc1101.SpiStrobe(CC1101_SFTX);
        }

        delay(5);
    }

    // delete rolling code . 0....n
    void delete_code()
    {
        ::Preferences preferences;
        preferences.begin("SomfyCover", false);
        // Keep the String alive for both statements below -- the previous
        // version took a `const char*` from a temporary String's .c_str()
        // and used it two lines later, after the temporary (and its buffer)
        // had already been destroyed. Undefined behavior, not just a warning.
        String path = rtsDevice->getConfigFilename();

        preferences.remove(path.c_str());

        ESP_LOGD("SomfyCover.h", "Deleted remote %i", remoteId);
        preferences.end();
    }

    cover::CoverTraits get_traits() override
    {
        auto traits = cover::CoverTraits();
        traits.set_is_assumed_state(false);
        traits.set_supports_position(true);
        traits.set_supports_stop(true); // Middle button of the remote
        // Debug/programming commands (program, delete rolling code, clear
        // prefs, ...) used to be dispatched by dragging this cover's tilt
        // slider to a specific number (11, 21, 61, ...). That put a
        // one-mistap-away path to wiping a shade's pairing in the same UI
        // control someone might nudge by accident. Those commands are now
        // exposed as their own `button:` entities (see SomfyCommandButton.h
        // and run_command() below) instead, so the tilt slider -- which
        // this component never used for real tilt anyway -- is removed.
        traits.set_supports_tilt(false);
        return traits;
    }

    // Debug/programming command dispatch, shared by every `button: -
    // platform: somfy` entity pointed at this cover (see
    // SomfyCommandButton::press_action()). See the cmd table comment near
    // the top of this file.
    void run_command(int xpos)
    {
        ::Preferences preferences;
        bool success;
        int ret;
        ESP_LOGI("SomfyCover.h", "Command xpos: %d", xpos);

        switch (xpos)
        {
        case 0:
            ESP_LOGI("SomfyCover.h", "Current rolling code is %d.", rtsDevice->readRemoteRollingCode());
            break;

        case 11:
            ESP_LOGI("SomfyCover.h", "Program mode");

            rtsDevice->sendCommandProg();
            break;

        case 16:
            ESP_LOGI("SomfyCover.h", "Program mode - grail");

            rtsDevice->sendCommandProgGrail();
            break;

        case 21:
            ESP_LOGI("SomfyCover.h", "Delete file");
            delete_code();
            break;

        case 50:
            ESP_LOGI("SomfyCover.h", "Long program mode");
            rtsDevice->sendCommandProg(20);
            break;

        case 61:
            ESP_LOGI("SomfyCover.h", "Clearing all values in Preference library");

            nvs_flash_erase(); // erase the NVS partition and...
            nvs_flash_init(); // initialize the NVS partition.

            success = preferences.begin("SomfyCover", false);
            if (success) {
                ESP_LOGW("SomfyCover.h", "Begin success");
            } else {
                ESP_LOGI("SomfyCover.h", "Begin fail");
            }

            ret = preferences.putUShort("test", 20);
            if (ret == 0) {
                ESP_LOGW("SomfyCover.h", "Error while test-writing.");
            } else {
                ESP_LOGI("SomfyCover.h", "Memory write success.");
            }

            preferences.end();
            break;

        // Debug commands
        case 90:
            setup();
            break;

        case 97:
            cc1101.SetTx();
            break;

        case 98:
            cc1101.setSidle();
            break;

        case 99:
            // Was hardcoded to pin 2 (the old Wemos-D1/plain-ESP32 GDO0
            // default) regardless of what's actually configured -- on this
            // board GDO0 is GPIO3, so this never touched the real TX pin.
            // Use the configured gdo0_pin instead.
            digitalWrite(gdo0_pin, HIGH);
            break;

        case 100:
            digitalWrite(gdo0_pin, LOW);
            break;

        default:
            break;
        }
    }

    void control(const cover::CoverCall& call) override
    {
        // This will be called every time the user requests a state change.

        ESP_LOGW("SomfyCover.h", "Using remote %d", REMOTE_FIRST_ADDR + index);
        ESP_LOGW("SomfyCover.h", "Remoteid %d", remoteId);
        ESP_LOGW("SomfyCover.h", "index %d", index);

        if (call.get_position().has_value()) {
            float pos = *call.get_position();
            // Write pos (range 0-1) to cover
            // ...
            int ppos = pos * 100;
            ESP_LOGD("SomfyCover.h", "get_position is: %d", ppos);

            if (ppos == 0) {
                ESP_LOGD("SomfyCover.h", "POS 0");
                Serial.println(invert_ ? "* Command Up (inverted)" : "* Command Down");

                if (invert_) rtsDevice->sendCommandUp();
                else         rtsDevice->sendCommandDown();

                pos = 0.01;
            } else if (ppos == 100) {
                ESP_LOGD("SomfyCover.h", "POS 100");
                Serial.println(invert_ ? "* Command Down (inverted)" : "* Command UP");

                if (invert_) rtsDevice->sendCommandDown();
                else         rtsDevice->sendCommandUp();

                pos = 0.99;
            } else {
                // In between position, set it to saved position
                ESP_LOGD("SomfyCover.h", "POS 50");
                Serial.println("* Command MY");

                rtsDevice->sendCommandStop();

                pos = 0.5;
            }

            // Publish new state
            this->position = pos;
            this->publish_state();
        }
        else if (call.get_stop()) {
            // User requested cover stop
            ESP_LOGD("SomfyCover", "get_stop");

            rtsDevice->sendCommandStop();

        }
    }
};

ELECHOUSE_CC1101 SomfyCover::cc1101;
std::queue<unsigned char> SomfyCover::_bufferQueue;
int SomfyCover::sck_pin = SOMFY_DEFAULT_SCK_PIN;
int SomfyCover::miso_pin = SOMFY_DEFAULT_MISO_PIN;
int SomfyCover::mosi_pin = SOMFY_DEFAULT_MOSI_PIN;
int SomfyCover::csn_pin = SOMFY_DEFAULT_CSN_PIN;
int SomfyCover::gdo0_pin = SOMFY_DEFAULT_GDO0_PIN;
int SomfyCover::gdo2_pin = SOMFY_DEFAULT_GDO2_PIN;


}  // namespace somfy
}  // namespace esphome