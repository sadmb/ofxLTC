//
//  ofxLTC.h
//
//  Created by 2bit on 2020/02/02.
//

#ifndef ofxLTC_h
#define ofxLTC_h


#include "decoder.h"
#include "encoder.h"
#include "ofSoundStream.h"
#include "ofSoundBuffer.h"
#include "ofUtils.h"
#include "ofThread.h"
#include "ofLog.h"


namespace ofx {
    namespace LTC {
        struct Timecode {
            LTCFrameExt raw_data;
            std::string timezone;
            std::uint16_t year;
            std::uint8_t month;
            std::uint8_t day;
            std::uint8_t hour;
            std::uint8_t min;
            std::uint8_t sec;
            std::uint8_t frame;
            bool reverse;
            
            std::string toString() const {
                return ofVAArgsToString("%04d/%02d/%02d[%s] %02d:%02d:%02d%c%02d",
                                        year, month, day, timezone.c_str(),
                                        hour, min, sec,
                                        (raw_data.ltc.dfbit ? '.' : ':'),
                                        frame);
            }
            
            float receivedTime;
        };
        struct Receiver {
            ~Receiver() {
                ltc_decoder_free(decoder);
                decoder = nullptr;
            }
            
            void setup(const ofSoundStreamSettings &settings,
                       std::size_t channel_offset = 0ul) {
                ofSoundStreamSettings settings_ = settings;
                settings_.setInListener(this);
                this->channel_offset = channel_offset;
                soundStream.setup(settings_);
                
                decoder = ltc_decoder_create(1920, 32);
                total = 0ul;
            }
            
            void onReceive(const std::function<void(Timecode)> &callback)
            { this->callback = callback; };
            
            std::vector<ofSoundDevice> getDeivceList() const
            { return soundStream.getDeviceList(); };
            
            void audioIn(ofSoundBuffer &buffer) {
                std::vector<std::uint8_t> buf;
                getBytePCM(buffer, buf);
                ltc_decoder_write(decoder, buf.data(), buf.size(), total);
                while(ltc_decoder_read(decoder, &frame)) {
                    Timecode timecode;
                    std::memcpy(&timecode.raw_data, &frame, sizeof(frame));
                    SMPTETimecode stime;
                    ltc_frame_to_time(&stime, &frame.ltc, LTC_USE_DATE);
                    
                    timecode.timezone = stime.timezone;
                    timecode.year = (stime.years < 67)
                                  ? (2000 + stime.years)
                                  : (1900 + stime.years);
                    timecode.month = stime.months;
                    timecode.day = stime.days;
                    timecode.hour = stime.hours;
                    timecode.min = stime.mins;
                    timecode.sec = stime.secs;
                    timecode.frame = stime.frame;
                    timecode.reverse = frame.reverse;
                    timecode.receivedTime = ofGetElapsedTimef();
                    callback(timecode);
                }
                total += buf.size();
            }

        protected:

            void getBytePCM(const ofSoundBuffer &buffer,
                            std::vector<std::uint8_t> &buf) const
            {
                buf.resize(buffer.size() / buffer.getNumChannels());
                std::size_t size = buffer.size();
                for(std::size_t i = 0; i < buf.size(); ++i) {
                    buf[i] = (buffer[buffer.getNumChannels() * i + channel_offset] + 1.0) * 127.5f;
                }
            }
            
            ofSoundStream soundStream;
            LTCDecoder *decoder;
            LTCFrameExt frame;
            std::size_t channel_offset;
            std::size_t total;
            std::function<void(Timecode)> callback{[](Timecode) {}};
        };
    
        class Sender  : public ofThread, public ofBaseSoundOutput {
        public:
            Sender() {
                setTimecode(0, 0, 0, 0);
            }
            virtual ~Sender() {
                if (encoder) {
                    ltc_encoder_free(encoder);
                    encoder = nullptr;
                    waitForThread();
                }
            }
            
            void start()
            {
                if (!encoder) return;

                if (!is_playing && pause_elapsed_time > 0) {
                    // resume
                    long paused_duration = ofGetElapsedTimeMillis() - pause_elapsed_time;
                    playback_start_elapsed_time += paused_duration;
                    pause_elapsed_time = 0;

                    is_playing = true;
                    ofLogNotice() << "[LTC] Resumed playback.";
                    return;
                }

                playback_start_elapsed_time = ofGetElapsedTimeMillis();

                startTimecode = currentTimecode;
                frames_already_advanced = 0;
                is_playing = true;

                ofLogNotice() << "[LTC] Starting playback.";
            }

            void stop()
            {
                if (is_playing) {
                    pause_elapsed_time = ofGetElapsedTimeMillis();
                    is_playing = false;
                }
            }

            bool isPlaying()
            {
                return is_playing;
            }
            void setup(const ofSoundStreamSettings &settings,
                       float fps_, bool drop_frame_ = false,
                       std::size_t channel_offset_ = 0ul,
                       enum LTC_TV_STANDARD standard_ = LTC_TV_525_60,
                       int ltc_flags_ = LTC_USE_DATE)
            {
                fps = fps_;
                currentTimecode.raw_data.ltc.dfbit = (int)drop_frame_;
                channel_offset = channel_offset_;
                
                ofSoundStreamSettings settings_ = settings;
                settings_.setOutListener(this);

                sampleRate = settings_.sampleRate;
                samplesPerFrame = static_cast<int>(ceil(sampleRate / fps));

                encoder = ltc_encoder_create(
                    static_cast<double>(sampleRate),
                    static_cast<double>(fps),
                    standard_,
                    ltc_flags_
                );

                if (!encoder) {
                    ofLogError() << "Failed to create LTC encoder";
                }

                ltcQueue.clear();

                soundStream.setup(settings_);
                if (!isThreadRunning()) {
                    startThread();
                }
            }

            void setTimecode(const Timecode &tc) {
                currentTimecode = tc;
            }

            void setTimecode(int hour_, int min_, int sec_, int frame_, int year_ = 0, int month_ = 0, int day_ = 0, std::string timezone_ = "+0900", bool drop_frame_ = false, bool reverse = false) {
                if(year_ <= 0)
                {
                    year_ = ofGetYear();
                }
                if(month_ <= 0 || month_ > 12)
                {
                    month_ = ofGetMonth();
                }
                if(day_ <= 0 || day_ > daysInMonth(year_, month_))
                {
                    day_ = ofGetDay();
                }
                currentTimecode.year = year_;
                currentTimecode.month = month_;
                currentTimecode.day = day_;
                currentTimecode.hour = hour_;
                currentTimecode.min = min_;
                currentTimecode.sec = sec_;
                currentTimecode.frame = frame_;
                currentTimecode.timezone = timezone_;
                currentTimecode.raw_data.ltc.dfbit = (int)drop_frame_;
                currentTimecode.reverse = reverse;
            }
            
            Timecode getTimecode()
            {
                return currentTimecode;
            }
            
            void updateTimecode()
            {
                currentTimecode.frame++;
                if (currentTimecode.frame >= fps) {
                    currentTimecode.frame = 0;
                    currentTimecode.sec++;
                    if (currentTimecode.sec >= 60) {
                        currentTimecode.sec = 0;
                        currentTimecode.min++;
                        if (currentTimecode.min >= 60) {
                            currentTimecode.min = 0;
                            currentTimecode.hour++;
                            if (currentTimecode.hour >= 24) {
                                currentTimecode.hour = 0;
                                currentTimecode.day++;

                                int dim = daysInMonth(currentTimecode.year, currentTimecode.month);
                                if (currentTimecode.day > dim) {
                                    currentTimecode.day = 1;
                                    currentTimecode.month++;
                                    if (currentTimecode.month > 12) {
                                        currentTimecode.month = 1;
                                        currentTimecode.year++;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            void audioOut(ofSoundBuffer &buffer) {
                mutex.lock();
                for (size_t i = 0; i < buffer.getNumFrames(); i++) {
                    float sample = 0.0f;

                    if (!ltcQueue.empty()) {
                        sample = ltcQueue.front();
                        ltcQueue.pop_front();
                    } else {
                        sample = 0.0f;
                    }

                    buffer[i * buffer.getNumChannels() + 0] = sample;
                    if (buffer.getNumChannels() > 1) {
                        buffer[i * buffer.getNumChannels() + 1] = 0.0f;
                    }
                }
                mutex.unlock();
            }
            
            bool isLeapYear(int year) {
                return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
            }

            int daysInMonth(int year, int month) {
                static const int daysPerMonth[12] = {
                    31, 28, 31, 30, 31, 30,
                    31, 31, 30, 31, 30, 31
                };

                if (month == 2) {
                    return isLeapYear(year) ? 29 : 28;
                } else if (month >= 1 && month <= 12) {
                    return daysPerMonth[month - 1];
                } else {
                    return 30; // fallback
                }
            }
        protected:
            void threadedFunction() override
            {
                while (isThreadRunning()) {
                    double ms_per_frame = 1000.0 / fps;
                    if (!is_playing) {
                        update();
                        sleep(1);
                        continue;
                    }

                    long now_ms = ofGetElapsedTimeMillis();
                    long elapsed_ms = now_ms - playback_start_elapsed_time;

                    if (elapsed_ms < 0) elapsed_ms = 0;

                    int frames_should_have_elapsed = int(elapsed_ms / ms_per_frame);
                    int frames_to_advance = frames_should_have_elapsed - frames_already_advanced;

                    if (frames_to_advance > 0 && frames_to_advance < 1000) {
                        for (int i = 0; i < frames_to_advance; i++) {
                            updateTimecode();
                        }
                        frames_already_advanced += frames_to_advance;
                    } else if (frames_to_advance < 0) {
                        ofLogWarning() << "[LTC] Negative frames_to_advance: " << frames_to_advance << ". Resetting to zero.";
                        frames_to_advance = 0;
                    } else if (frames_to_advance >= 1000) {
                        ofLogWarning() << "[LTC] frames_to_advance too large: " << frames_to_advance << ". Resetting.";
                        frames_already_advanced = frames_should_have_elapsed;
                    }

                    update();

                    sleep(1);
                }
            }

            void update() {
                SMPTETimecode smpte;
                memset(&smpte, 0, sizeof(smpte));

                if(encoder->flags & LTC_USE_DATE) {
                    strncpy(smpte.timezone, currentTimecode.timezone.c_str(), 6);
                    smpte.years  = currentTimecode.year % 100;
                    smpte.months = currentTimecode.month;
                    smpte.days   = currentTimecode.day;
                }
                smpte.hours  = currentTimecode.hour;
                smpte.mins   = currentTimecode.min;
                smpte.secs   = currentTimecode.sec;
                smpte.frame  = currentTimecode.frame;
                
                ltc_encoder_set_timecode(encoder, &smpte);
                LTCFrame frame;
                ltc_encoder_get_frame(encoder, &frame);
                frame.dfbit = currentTimecode.raw_data.ltc.dfbit;
                ltc_encoder_set_frame(encoder, &frame);

                ltc_encoder_encode_frame(encoder);

                size_t numSamples = 0;
                ltcsnd_sample_t *samples = ltc_encoder_get_bufptr(encoder, (int*)&numSamples, 1);
                if (!samples || numSamples == 0) {
                    return;
                }

                mutex.lock();
                for (size_t i = 0; i < numSamples; i++) {
                    float val = (float(samples[i]) - 128.f) / 127.f;
                    ltcQueue.push_back(val);
                }
                mutex.unlock();
            }
            
            ofSoundStream soundStream;
            LTCEncoder* encoder = nullptr;
            float fps = 30.0f;
            int sampleRate = 48000;
            int samplesPerFrame = 0;
            size_t channel_offset = 0;
            bool is_playing = false;
            
            long frames_already_advanced = 0;
            long playback_start_elapsed_time = 0;
            long pause_elapsed_time = 0;
            
            std::deque<float> ltcQueue;

            Timecode currentTimecode, startTimecode;
        };
    };
};

namespace ofxLTC = ofx::LTC;
using ofxLTCTimecode = ofxLTC::Timecode;
using ofxLTCReceiver = ofxLTC::Receiver;
using ofxLTCSender = ofxLTC::Sender;

#endif /* ofxLTC_h */
