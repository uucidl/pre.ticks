#include <fstream>
#include <future>
#include <sstream>
#include <string>
#include <vector>

#include <cmath>
#include <cstdio> // for printf

#include <GL/glew.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"

#include "stb_image.c"
#include "stb_image_write.h"
#include "stb_perlin.h"
#include "stb_truetype.h"
#include "ujdecode.h"

#pragma clang diagnostic pop

#include <micros/api.h>

#include "../common/main_types.h"
#include "../common/shader_types.h"
#include "../common/buffer_types.h"
#include "../common/fragops_types.h"

const double TAU = 6.28318530717958647692528676655900576839433879875021;

class Phasers
{
public:
        size_t add(double frequency, double offset = 0.0)
        {
                size_t const id = data.size();
                data.emplace_back(offset, frequency / 48000.0);
                return id;
        }

        size_t add_follower(size_t main, double ratio = 1.0, double offset = 0.0)
        {
                size_t const id = add(0.0, offset);
                followers.emplace_back(id, main, ratio);
                return id;
        }

        void change(size_t phaser, double frequency)
        {
                data[phaser].increment = frequency / 48000.0;
        }

        void offset(size_t phaser, double offset)
        {
                data[phaser].phase = offset;
        }

        double get(int phaser) const
        {
                return data[phaser].phase;
        }

        double get_radians(int phaser) const
        {
                return data[phaser].phase * TAU;
        }

        void advance()
        {
                for (auto& follower : followers) {
                        data[follower.id].increment = data[follower.main].increment * follower.ratio;
                }

                for (auto& phaser : data) {
                        phaser.phase =
                                fmod(phaser.phase + phaser.increment, 1.0);
                }
        }

private:
        struct state {
                double phase;
                double increment;
                state(double phase, double increment) :
                        phase(phase),
                        increment(increment) {}
        };
        std::vector<state> data;

        struct follower_state {
                size_t id;
                size_t main;
                double ratio;

                follower_state(size_t id, size_t main, double ratio) :
                        id(id),
                        main(main),
                        ratio(ratio) {}
        };

        std::vector<follower_state> followers;
};

static double sinexpenv(double phase, double attack_speed, double decay_speed)
{
        //double const attack = fmax(0.0, 1.0 + log10(fmin(1.0, attack_speed * phase)));
        double const attack_dur = 1.0 / attack_speed;
        double const attack = fmax(0.0, sin(fmin (attack_dur,
                                            phase) * TAU * attack_speed / 4.0));
        double const decay = fmin(1.0,
                                  exp(-decay_speed * phase + decay_speed/attack_speed));
        return attack * decay * fmin(1.0, 1000.0 * (1.0 - phase));
}

static void phaser_tweak(Phasers& phasers,
                         size_t const tweaked,
                         size_t const original,
                         size_t const shifter)
{
        double const original_phase = phasers.get(original);
        if (phasers.get(shifter) == 0.0) {
                phasers.offset(tweaked, original_phase);
        }
        phasers.change(tweaked, original);
}

static double phaser_fbmodulate(Phasers& phasers,
                                size_t const main,
                                size_t const modulator,
                                double const frequency,
                                double const cm_frequency_ratio,
                                double const modulation_index,
                                double const feedback)
{
        double const osc_radians = phasers.get_radians(main);
        double const mod_radians = phasers.get_radians(modulator);
        double const modulation = modulation_index * sin(mod_radians);
        double const main_freq = frequency + modulation;

        phasers.change(modulator,
                       feedback * sin(osc_radians) + frequency / cm_frequency_ratio);
        phasers.change(main, main_freq);

        return main_freq;
}

static double phaser_n(double phase, double q)
{
        return fmod(q * phase, 1.0);
}

extern void render_next_2chn_48khz_audio(uint64_t time_micros,
                int const sample_count, double left[/*sample_count*/],
                double right[/*sample_count*/])
{
        static Phasers phasers;

        static struct SharedPhasers {
                size_t shifter = phasers.add(48000.0 / 64.0);
                size_t measure = phasers.add(0.50);
                size_t sometime = phasers.add_follower(measure, 1.0 / 16.0 / 8.0);
        } shared_phasers;

        static struct KickPhasers {
                size_t a = phasers.add_follower(shared_phasers.measure, 4.0);
                size_t aa = phasers.add_follower(a);
                size_t osc = phasers.add(50.0);
        } kick_phasers;

        struct Kick {
                double freq_env_base = 50.0;
                double freq_env_amp = 1500.0;
                double freq_env_accel = 1000.0;
                double freq_env_decay = 80.0;
                double amplitude_env_accel = 2000.0;
                double amplitude_env_decay = 4.0;
        } kick;

        static struct BounceKickPhasers {
                size_t osc = phasers.add_follower(kick_phasers.osc);
        } bounce_kick_phasers;

        struct BounceKick {
                double freq_env_base = 50.0;
                double amplitude_env_accel = 80.0;
                double amplitude_env_decay = 20.0;
        } bounce_kick;

        static struct SnarePhasers {
                size_t a = phasers.add_follower(kick_phasers.a, 1.0/2.0, 0.50);
                size_t osc = phasers.add(180.0);
                size_t mod_osc = phasers.add(90.0);
        } snare_phasers;

        struct Snare {
                double freq_env_base = 50.0 * 1.5;
                double freq_env_amp = 3000.0;
                double freq_env_accel = 1000.0;
                double freq_env_decay = 90.0;
                double amplitude_env_accel = 1200.0;
                double amplitude_env_decay = 13.0;
                double modulator_freq_ratio = 1.0 / sqrt(1.5);
                double modulator_index = 800.0;
                double feedback = 1600.0;
        } snare;


        struct Hihat {
                double freq_env_base = 50.0 * 4.0;
                double freq_env_amp = 600.0;
                double freq_env_accel = 1000.0;
                double freq_env_decay = 90.0;
                double amplitude_env_accel = 1200.0;
                double amplitude_env_decay = 13.0;
                double modulator_freq_ratio = 1.0 / sqrt(3.0);
                double modulator_index = 800.0;
                double feedback = 0.97;
        } hihat;

        static struct HihatPhasers {
                size_t a = phasers.add_follower(kick_phasers.a, 1.0/2.0, 0.50);
                size_t osc = phasers.add(180.0);
                size_t mod_osc = phasers.add(90.0);
        } hihat_phasers;

        static struct BassPhasers {
                size_t b = phasers.add(4.0);
                size_t ba = phasers.add_follower(b);
                size_t osc = phasers.add(110.0);
                size_t sweep = phasers.add(1.0/32.0);
                size_t modulator_osc = phasers.add(110.0);
        } bass_phasers;

        struct Bass {
                double modulator_freq_ratio = 0.5;
                double modulator_amp = 3.0;
                double freq_env_base = 110.0;
                double freq_env_amp = 1500.0;
                double freq_env_accel = 400.0;
                double freq_env_decay = 40.0;
                double amplitude_env_accel = 8.0;
                double amplitude_env_decay = 3.0;
        } bass;

        static struct MidPhasers {
                size_t m = phasers.add(1.0/16.0, 0.750);
                size_t root_osc = phasers.add(220.0);
                size_t modulator_osc = phasers.add(220.0);
                size_t detuned_osc = phasers.add_follower(root_osc, 1.0037);
                size_t major[2];
                size_t minor[2];

                MidPhasers()
                {
                        major[0] = phasers.add_follower(root_osc, 5.0 / 4.0);
                        minor[1] = phasers.add_follower(root_osc, 6.0 / 4.0);
                        minor[0] = phasers.add_follower(root_osc, 12.0 / 10.0);
                        minor[1] = phasers.add_follower(root_osc, 15.0 / 10.0);
                }
        } mid_phasers;

        struct Mid {
                double modulator_freq_ratio = 2.00;
                double modulator_amp = 15.0;
                double modulator_fb = 0.2570;
                double amplitude_env_accel = 5.0;
                double amplitude_env_decay = 6.0;
        } mid;

        bool kick_track = true;
        bool bounce_kick_track = true;
        bool snare_track = true;
        bool hihat_track = true;
        bool mid_track = true;

        double const kick_gain = 0.5;
        double const kick_bounce_gain = 0.5;
        double const snare_gain = 0.4;
        double const hihat_gain = 0.5;
        double const mid_gain = 0.25;
        double const master_gain = 0.5;

        double const bpm = 133.0;
        phasers.change(shared_phasers.measure, bpm / 120.0 * 0.50);

        for (int i = 0; i < sample_count; i++) {
                left[i] = 0.0;
                right[i] = 0.0;

                bool all_measures_but_last =
                        floor(fmod(1.0 +
                                   phasers.get(shared_phasers.sometime) * 16.0,
                                   16.0)) > 0.0;

                kick_track = all_measures_but_last;
                bounce_kick_track = kick_track;

                if (kick_track) {
                        auto const& note_phaser = kick_phasers.a;
                        auto const& expression_phaser = kick_phasers.aa;
                        phaser_tweak(phasers,
                                     expression_phaser,
                                     note_phaser,
                                     shared_phasers.shifter);

                        auto const& note_phase = phasers.get(note_phaser);
                        auto const& expression_phase = phasers.get(expression_phaser);
                        auto const& params = kick;
                        auto const& voice = kick_phasers;
                        auto const& gain = kick_gain;

                        double const amplitude =
                                sinexpenv(expression_phase,
                                          params.amplitude_env_accel,
                                          params.amplitude_env_decay);

                        double const freq =
                                params.freq_env_base +
                                params.freq_env_amp * sinexpenv(note_phase,
                                                                params.freq_env_accel,
                                                                params.freq_env_decay);

                        phasers.change(voice.osc, freq);

                        double const osc_radians = phasers.get_radians(voice.osc);
                        left[i] += gain * amplitude * cos(osc_radians);
                        right[i] += gain * amplitude * cos(osc_radians);
                }

                if (bounce_kick_track) {
                        double const measure = phasers.get(shared_phasers.measure);
                        double const beat = 16.0 * measure;
                        double const first = (beat >= 3.0
                                              && beat < 4.0) ? phaser_n(measure, 16.0) : 0.0;
                        double const second = (beat >= 6.0
                                               && beat < 8.0) ? phaser_n(measure, 8.0) : 0.0;

                        auto const& note_phase = first + second;
                        auto const& params = bounce_kick;
                        auto const& voice = bounce_kick_phasers;
                        auto const& gain = kick_bounce_gain;

                        double const amplitude =
                                sinexpenv(note_phase,
                                          params.amplitude_env_accel,
                                          params.amplitude_env_decay);

                        double const osc_radians = phasers.get_radians(voice.osc);
                        left[i] += gain * amplitude * cos(osc_radians);
                        right[i] += gain * amplitude * cos(osc_radians);
                }

                if (snare_track) {
                        auto const& note_phaser = snare_phasers.a;

                        auto const& note_phase = phasers.get(note_phaser);
                        auto const& params = snare;
                        auto const& voice = snare_phasers;
                        auto const& gain = snare_gain;

                        double const amplitude =
                                sinexpenv(note_phase,
                                          params.amplitude_env_accel,
                                          params.amplitude_env_decay);
                        double const freq =
                                params.freq_env_base +
                                params.freq_env_amp * sinexpenv(note_phase,
                                                                params.freq_env_accel,
                                                                params.freq_env_decay);

                        phaser_fbmodulate(phasers,
                                          voice.osc,
                                          voice.mod_osc,
                                          freq,
                                          params.modulator_freq_ratio,
                                          params.modulator_index,
                                          params.feedback);

                        double const osc_radians = phasers.get_radians(voice.osc);
                        left[i] += gain * amplitude * cos(osc_radians) * sin(osc_radians);
                        right[i] += gain * amplitude * cos(osc_radians) * sin(osc_radians);
                }

                if (hihat_track) {
                        double const measure = phasers.get(shared_phasers.measure);
                        double const beat    = floor(measure * 16.0);
                        double const mask    = fmod(beat + 2.0, 4.0) > 0.0 ? 0.0 : 1.0;
                        double const open_mask =
                                floor(fmod(beat + 7.0, 16.0) / 2.0) > 0.0 ? 0.0 : 1.0;
                        double const phase   =
                                fmod(mask * phaser_n(measure, 16.0) +
                                     open_mask * phaser_n(measure, 8.0), 1.0);

                        auto const& note_phase = phase;
                        auto const& params = hihat;
                        auto const& voice = hihat_phasers;
                        auto const& gain = hihat_gain;

                        double const amplitude =
                                sinexpenv(note_phase,
                                          params.amplitude_env_accel,
                                          params.amplitude_env_decay);
                        double const freq =
                                params.freq_env_base +
                                params.freq_env_amp * sinexpenv(note_phase,
                                                                params.freq_env_accel,
                                                                params.freq_env_decay);

                        phaser_fbmodulate(phasers,
                                          voice.osc,
                                          voice.mod_osc,
                                          freq,
                                          params.modulator_freq_ratio,
                                          params.modulator_index,
                                          params.feedback);

                        double const osc_radians = phasers.get_radians(voice.osc);

                        left[i] += gain * amplitude * cos(osc_radians) * sin(osc_radians);
                        right[i] += gain * amplitude * cos(osc_radians) * sin(osc_radians);
                }

                if (mid_track) {
                        auto const& note_phaser = mid_phasers.m;
                        auto const& params = mid;
                        auto const& voice = mid_phasers;
                        auto const& gain = mid_gain;

                        double const note_phase = phasers.get(note_phaser);
                        double const section = fmod(floor(shared_phasers.sometime * 16.0 * 4.0), 2.0);
                        double const minor_phase = (section == 0.0) ? note_phase : 0.0;
                        double const major_phase = (section == 1.0) ? note_phase : 0.0;
                        double amplitude = sinexpenv(note_phase,
                                                     params.amplitude_env_accel,
                                                     params.amplitude_env_decay);

                        phaser_fbmodulate(phasers,
                                          voice.root_osc,
                                          voice.modulator_osc,
                                          50.0 * 5,
                                          params.modulator_freq_ratio,
                                          amplitude * params.modulator_amp,
                                          params.modulator_fb);

                        for (auto const& phaser : voice.major) {
                                double amplitude = sinexpenv(major_phase,
                                                             params.amplitude_env_accel,
                                                             params.amplitude_env_decay);

                                double const osc = amplitude * cos(phasers.get_radians(phaser));

                                left[i] += gain * osc;
                                right[i] += gain * osc;
                        }

                        for (auto const& phaser : voice.minor) {
                                double amplitude = sinexpenv(minor_phase,
                                                             params.amplitude_env_accel,
                                                             params.amplitude_env_decay);

                                double const osc = amplitude * cos(phasers.get_radians(phaser));

                                left[i] += gain * osc;
                                right[i] += gain * osc;
                        }

                        double const osc = amplitude * cos(phasers.get_radians(voice.root_osc));

                        double const detuned_osc = amplitude * cos(phasers.get_radians(
                                                           voice.detuned_osc));

                        left[i] += gain * (0.55 * osc + 0.45 * detuned_osc);
                        right[i] += gain * (0.45 * osc + 0.55 * detuned_osc);
                }

                left[i] *= master_gain;
                right[i] *= master_gain;

                phasers.advance();
        }
}

std::string dirname(std::string path)
{
        return path.substr(0, path.find_last_of("/\\"));
}

extern void render_next_gl3(uint64_t time_micros)
{
        static class DoOnce : public DisplayThreadTasks, public FileSystem
        {
        public:
                void add_task(std::function<bool()>&& task)
                {
                        std::lock_guard<std::mutex> lock(tasks_mtx);
                        tasks.emplace_back(task);
                }

                DoOnce() : base_path(dirname(__FILE__)), shader_loader(*this, *this)
                {
                        printf("OpenGL version %s\n", glGetString(GL_VERSION));
                        printf("Watching files at %s\n", base_path.c_str());
                        shader_loader.load_shader("main.vs", "main.fs", [=](ShaderProgram&& input) {
                                shader = std::move(input);
                                position_attr = glGetAttribLocation(shader.ref(), "position");
                        });
                        float vertices[] = {
                                0.0f, 0.0f,
                                0.0f, 1.0f,
                                1.0f, 1.0f,
                                1.0f, 0.0f,
                        };

                        GLuint indices[] = {
                                0, 1, 2, 2, 3, 0
                        };

                        {
                                WithArrayBufferScope scope(vbo_vertices);
                                glBufferData(GL_ARRAY_BUFFER, sizeof vertices, vertices, GL_STREAM_DRAW);
                        }

                        {
                                WithElementArrayBufferScope scope(vbo_indices);
                                glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof indices, indices,
                                             GL_STREAM_DRAW);
                        }

                        printf("defining vertex array %d\n", vao_quad.ref);
                        {
                                WithVertexArrayScope vascope(vao_quad);

                                glEnableVertexAttribArray(position_attr);

                                glBindBuffer(GL_ARRAY_BUFFER, vbo_vertices.ref);
                                glVertexAttribPointer(position_attr, 2, GL_FLOAT, GL_FALSE, 0, 0);

                                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_indices.ref);
                        }
                }

                std::ifstream open_file(std::string relpath) const
                {
                        auto stream = std::ifstream(base_path + "/" + relpath);

                        if (stream.fail()) {
                                throw std::runtime_error("could not load file at " + relpath);
                        }

                        return stream;
                }

                void run()
                {
                        std::lock_guard<std::mutex> lock(tasks_mtx);
                        for (auto& task : tasks) {
                                std::future<bool> future = task.get_future();
                                task();
                                future.get();
                        }
                        tasks.clear();
                }

                std::string base_path;
                ShaderProgram shader;
                Buffer vbo_vertices;
                Buffer vbo_indices;
                VertexArray vao_quad;
                GLuint position_attr;

                std::mutex tasks_mtx;
                std::vector<std::packaged_task<bool()>> tasks;
                ShaderLoader shader_loader;
        } resources;

        resources.run();

        glClearColor (0.2f, 0.2f, 0.3f, 0.0f);
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


        {
                WithVertexArrayScope vascope(resources.vao_quad);
                WithBlendEnabledScope blend(GL_SRC_COLOR, GL_DST_COLOR);
                WithShaderProgramScope with_shader(resources.shader);
                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
                resources.shader.validate();
        }
}

int main (int argc, char** argv)
{
        (void) argc;
        (void) argv;

        runtime_init();

        return 0;
}
