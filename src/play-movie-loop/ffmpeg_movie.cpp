// # Movie playback using ffmpeg
//
// Not thread-safe, use only from one thread.
//
// Define UU_MOVIE_PLAYERS_IMPLEMENTATION to emit the implementation.
//
// ImportLib(avformat): demuxing (i.e. decoding container formats)
// ImportLib(avcodec): decoding (i.e. decompression of audio/video data)
// ImportLib(avutil): utilities for ffmpeg
// ImportLib(swscale): scaling and pixel conversion
//
// TODO(nicolas): we actually want to play loops, not movies

#ifndef uu_internal_symbol
#define uu_internal_symbol static
#endif

// marks all exported symbols
#ifndef uu_movie_players_api
#define uu_movie_players_api uu_internal_symbol
#endif

#ifndef UU_MOVIE_PLAYERS_PROTOTYPES
#define UU_MOVIE_PLAYERS_PROTOTYPES

#include <memory>

namespace uu_movie_players
{

/// @p [[temporary_arena, temporary_arena_size)) defines a region used
/// for temporary allocations.
uu_movie_players_api
void init(uint8_t* temporary_arena, size_t temporary_arena_size);

namespace details
{
struct QueueDescription;
uu_movie_players_api QueueDescription* make_queue_description();
}

struct Queue {
        const std::unique_ptr<details::QueueDescription> queue {
                details::make_queue_description() };
};

uu_movie_players_api
void enqueue_url(Queue* queue, char const* url,
                 size_t url_size);

struct Frame {
        uint64_t ts_micros;
        uint8_t* data;
        uint16_t width;
        uint16_t height;
        uint16_t aspect_ratio_numerator;
        uint16_t aspect_ratio_denominator;
};

uu_movie_players_api
bool decode_step(Queue* queue_container,
                 uint8_t* result_arena,
                 size_t result_arena_size, Frame* output);
} // uu_movie_players namespace

// Customization

// UU_MOVIE_PLAYERS_LOGFN(utf8_sym, size_sym): your logging function
// UU_MOVIE_PLAYERS_TRACE_VALUE(name_sym, double_sym): a data tracing function
// UU_MOVIE_PLAYERS_TRACE_STRING(name_sym, string_sym): a data tracing function
#endif // UU_MOVIE_PLAYERS_PROTOTYPES


#ifdef UU_MOVIE_PLAYERS_IMPLEMENTATION

#include <vector>

#define UU_MOVIE_PLAYERS_GLOBAL static
#define UU_MOVIE_PLAYERS_INTERNAL static

#include <cstdint>

namespace uu_movie_players
{

// Importing ffmpeg
namespace FFMPEG
{
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libswscale/swscale.h>
}
}

namespace std
{
using namespace ::std;
}

#ifndef UU_MOVIE_PLAYERS_LOGFN
#include <cstdio>
static inline void stdio_log(const char* utf8, size_t size)
{
        fwrite(utf8, size, 1, stdout);
        fflush(stdout);
}

#define UU_MOVIE_PLAYERS_LOGFN(utf8_sym, size_sym) stdio_log(utf8_sym, size_sym)
#endif

static inline void log(char const* str, size_t size)
{
        UU_MOVIE_PLAYERS_LOGFN(str, size);
}

static inline void log_zstr(char const* zstr)
{
        log(zstr, strlen(zstr));
}

static inline void log_formatted(char const* pattern_zstr, ...)
{
        va_list args;
        va_start(args, pattern_zstr);
        char buffer[4096];
        (void) vsnprintf(buffer, sizeof buffer, pattern_zstr, args);
        va_end(args);
        log_zstr(buffer);
}

#if !defined(UU_MOVIE_PLAYERS_TRACE_VALUE) || !defined(UU_MOVIE_PLAYERS_TRACE_STRING)
UU_MOVIE_PLAYERS_INTERNAL struct trace_output {
        trace_output()
        {
                auto filepath = "./trace.out";
                fd = fopen(filepath, "wb");
                log_formatted("directing data traces to %s\n", filepath);
        }
        ~trace_output()
        {
                fclose(fd);
                fd = nullptr;
        }
        FILE* fd = nullptr;
        uint64_t ts = 0;
} trace_output;
#endif

#ifndef UU_MOVIE_PLAYERS_TRACE_VALUE
#include <cstdio>

static inline void trace_value(const char* name, double value)
{
        fprintf(trace_output.fd,
                "value:%" PRIu64 ":%s: %f\n",
                trace_output.ts++,
                name,
                value);
        fflush(trace_output.fd);
}

#define UU_MOVIE_PLAYERS_TRACE_VALUE(name_sym, double_value_sym) \
        trace_value(#name_sym, double(double_value_sym))
#endif // UU_MOVIE_PLAYERS_TRACE_VALUE

#ifndef UU_MOVIE_PLAYERS_TRACE_STRING
static inline void trace_string(const char* name, const char* str)
{
        fprintf(trace_output.fd,
                "value:%" PRIu64 ":%s: %s\n",
                trace_output.ts++,
                name,
                str);
        fflush(trace_output.fd);
}

#define UU_MOVIE_PLAYERS_TRACE_STRING(name_sym, string_sym) \
        trace_string(#name_sym, string_sym)
#endif // UU_MOVIE_PLAYERS_TRACE_STRING

#ifdef __clang__
#define UU_MOVIE_PLAYERS_DEBUGBREAK asm("int3");
#endif


static inline void log_averror(int averror)
{
        char error_string[4096];
        FFMPEG::av_strerror(averror, error_string, sizeof error_string);
        log_formatted("Encountered error: %d:'%s'\n", averror, error_string);
}

static inline void log_av(FFMPEG::AVRational rational)
{
        log_formatted("(%d / %d)", rational.num, rational.den);
}

struct byte_range {
        uint8_t* start;
        size_t size;
};

struct arena {
        byte_range range;
        size_t limit;
};

UU_MOVIE_PLAYERS_INTERNAL arena global_temporary_arena;

UU_MOVIE_PLAYERS_INTERNAL
byte_range push_bytes(arena* arena, size_t block_size)
{
        if (block_size > arena->limit ||
            arena->limit - block_size < arena->range.size) {
                // TODO(nicolas): should that trigger new dynamic
                // arena allocation?
                log_zstr("Not enough memory");
                UU_MOVIE_PLAYERS_DEBUGBREAK;
                return {};
        }
        byte_range block;
        block.start = arena->range.start + arena->range.size;
        block.size = block_size;
        arena->range.size += block_size;
        return block;
}

UU_MOVIE_PLAYERS_INTERNAL
char* copy_zstr(char const* str, size_t str_size, arena* arena)
{
        auto block = push_bytes(arena, 1+str_size);
        if (block.size == 0) {
                return nullptr;
        }
        memcpy(block.start, str, str_size);
        block.start[str_size] = '\0';
        return (char*)block.start;
}


uu_movie_players_api
void init(uint8_t* temporary_arena, size_t temporary_arena_size)
{
        FFMPEG::av_register_all();
        FFMPEG::av_log_set_level(AV_LOG_WARNING);
        global_temporary_arena.range.start = temporary_arena;
        global_temporary_arena.range.size = 0;
        global_temporary_arena.limit = temporary_arena_size;
        log_formatted("using avformat %d, %s\n",
                      FFMPEG::avformat_version(),
                      FFMPEG::avformat_license());
        log_formatted("using avcodec %d, %s\n",
                      FFMPEG::avcodec_version(),
                      FFMPEG::avcodec_license());
        log_formatted("using swscale %d, %s\n",
                      FFMPEG::swscale_version(),
                      FFMPEG::swscale_license());
}

namespace details
{

template <typename Proc>
struct scope_guard {
        scope_guard(Proc proc) : cleanup_proc(proc) {}
        // TODO(nicolas): do we prevent the scope_guard from being
        // executed after being moved from?
        scope_guard(scope_guard&& x) = default;
        scope_guard(scope_guard&) = delete;
        ~scope_guard()
        {
                cleanup_proc();
        };
        Proc cleanup_proc;
};

template <typename Proc>
[[gnu::warn_unused_result]] scope_guard<Proc>
defer(Proc proc)
{
        return scope_guard<Proc>(proc);
}

struct avresources_header {
        FFMPEG::AVFormatContext* format_context = nullptr;
        FFMPEG::AVCodecContext* decoding_context = nullptr;
        FFMPEG::SwsContext* conversion_context = nullptr;
        int stream_index = -1;
};

struct avresources {
        avresources_header underlying;

        avresources() = default;
        avresources(avresources&& x)
        {
                underlying = x.underlying;
                x.underlying = {};
        }
        avresources_header const& get() const
        {
                return underlying;
        }
        ~avresources()
        {
                sws_freeContext(underlying.conversion_context);
                avcodec_free_context(&underlying.decoding_context);
                avformat_close_input(&underlying.format_context);
                underlying = {};
        }
};

struct source {
        avresources av;
        FFMPEG::AVPixelFormat output_format;
        uint64_t frame_last_ts_micros;
        uint64_t frame_origin_ts_micros;

};

struct QueueDescription {
        std::vector<source> sources;
        size_t source_index = 0;
};

uu_movie_players_api
QueueDescription* make_queue_description()
{
        return new QueueDescription;
}

}

uu_movie_players_api
void enqueue_url(Queue* queue, char const* url,
                 size_t url_size)
{
        auto print = log_formatted;
        FFMPEG::AVFormatContext* format_context = nullptr; // allocate one for me
        // TODO(nicolas): not thread safe obviously
        auto original_arena = global_temporary_arena;
        auto arena_cleanup = details::defer([&]() {
                global_temporary_arena = original_arena;
        });
        char const* url_zstr = copy_zstr(url, url_size, &global_temporary_arena);

        auto av_open_error = FFMPEG::avformat_open_input(
                                     &format_context,
                                     url_zstr,
                                     nullptr,
                                     nullptr);
        auto close_stream = details::defer([&]() {
                FFMPEG::avformat_close_input(&format_context);
        });
        if (av_open_error != 0) {
                print("with input file: %s\n", url_zstr);
                log_averror(av_open_error);
                UU_MOVIE_PLAYERS_DEBUGBREAK;
                return;
        }

        // Necessary for formats without header i.e. w/
        // AVFMTCTX_NOHEADER: for instance on `.mpg` files before
        // calling this function, the number of streams come out as
        // 0.
        auto av_stream_info_error = FFMPEG::avformat_find_stream_info(format_context,
                                    nullptr);
        if (av_stream_info_error != 0) {
                print("could not find stream info");
                log_averror(av_stream_info_error);
                UU_MOVIE_PLAYERS_DEBUGBREAK;
                return;
        }

        if (0 == format_context->nb_streams) {
                print("no stream in source url");
                UU_MOVIE_PLAYERS_DEBUGBREAK;
                return;
        }

        // enumerate hardware accelerators
        {
                FFMPEG::AVHWAccel* first = FFMPEG::av_hwaccel_next(nullptr);
                print("AV_PIX_FMT_VDAU_H264: %d\n", FFMPEG::AV_PIX_FMT_VDPAU_H264);
                print("AV_PIX_FMT_VDAU_MPEG1: %d\n", FFMPEG::AV_PIX_FMT_VDPAU_MPEG1);
                print("AV_PIX_FMT_VDAU_MPEG2: %d\n", FFMPEG::AV_PIX_FMT_VDPAU_MPEG2);
                print("AV_PIX_FMT_VDAU_WMV3: %d\n", FFMPEG::AV_PIX_FMT_VDPAU_WMV3);
                print("AV_PIX_FMT_VDAU_VC1: %d\n", FFMPEG::AV_PIX_FMT_VDPAU_VC1);
                print("AV_PIX_FMT_CUDA: %d\n", FFMPEG::AV_PIX_FMT_CUDA);
                print("AV_PIX_FMT_VDA: %d\n", FFMPEG::AV_PIX_FMT_VDA);
                print("AV_PIX_FMT_VIDEOTOOLBOX: %d\n", FFMPEG::AV_PIX_FMT_VIDEOTOOLBOX);
                print("AV_PIX_FMT_VDA_VLD: %d\n", FFMPEG::AV_PIX_FMT_VDA_VLD);
                while (first) {
                        FFMPEG::AVHWAccel* hwaccel = first;
                        print("hwaccel ");
                        print(", name: %s", hwaccel->name);
                        print(", type: %d", hwaccel->type);
                        print(", id: %d", hwaccel->id);
                        print(", pix_fmt: %d", hwaccel->pix_fmt);
                        print(", capabilities: %d", hwaccel->capabilities);
                        print("\n");
                        first = FFMPEG::av_hwaccel_next(first);
                }
        }

        print("---\n");
        print("opened url: %s\n", url_zstr);
        print("flags: %d\n", format_context->ctx_flags);
        print("\tAVFMTCTX_NOHEADER: %d\n",
              format_context->ctx_flags & AVFMTCTX_NOHEADER);
        print("nb_streams: %d\n", format_context->nb_streams);

        FFMPEG::AVCodec* video_codec = nullptr;
        auto video_stream_index = FFMPEG::av_find_best_stream(format_context,
                                  FFMPEG::AVMEDIA_TYPE_VIDEO,
                                  -1,
                                  -1,
                                  &video_codec,
                                  0);
        if (video_stream_index < 0) {
                print("can't find a good stream\n");
                UU_MOVIE_PLAYERS_DEBUGBREAK;
                return;
        }

        print("automatic selected video stream: %d\n", video_stream_index);
        FFMPEG::AVStream* video_stream = format_context->streams[video_stream_index];
        print("\tstart_time: %" PRId64 "\n", video_stream->start_time);
        print("\tduration: %" PRId64 "\n", video_stream->duration);
        print("\tnb_frames: %d\n", video_stream->nb_frames);
        print("\tdiscard: %d\n", video_stream->discard);
        print("\tsample_aspect_ratio: ");
        log_av(video_stream->sample_aspect_ratio);
        print("\n");

        print("Video codec: ");
        {
                print(", name: %s", video_codec->name);
                print(", long_name: %s", video_codec->long_name);
                print(", capabilities: %d", video_codec->capabilities);
                if (video_codec->capabilities & CODEC_CAP_HWACCEL) {
                        print(", hwaccel");
                }
                if (video_codec->capabilities & CODEC_CAP_EXPERIMENTAL) {
                        print(", experimental");
                }
                if (video_codec->pix_fmts) {
                        for (FFMPEG::AVPixelFormat const * pixel_format = video_codec->pix_fmts;
                             *pixel_format != -1;
                             ++pixel_format) {
                                print(", pixel format: %d", *pixel_format);
                        }
                }
                print(", next codec: %p", video_codec->next);
                print("\n");
        };

        FFMPEG::AVCodecContext* video_codec_context;
        video_codec_context = FFMPEG::avcodec_alloc_context3(video_codec);
        if (!video_codec_context) {
                print("failed to allocate codec context\n");
                return;
        }
        auto free_codec = details::defer([&]() {
                FFMPEG::avcodec_free_context(&video_codec_context);
        });

        auto context_error = FFMPEG::avcodec_parameters_to_context
                             (video_codec_context, video_stream->codecpar);
        if (context_error != 0) {
                print("failed to create codec context\n");
                return;
        }

        FFMPEG::AVDictionary* codec_options = nullptr;
        const auto codec_error = FFMPEG::avcodec_open2(video_codec_context,
                                 video_codec,
                                 &codec_options);
        if (codec_error < 0) {
                print("error opening codec: ");
                log_averror(codec_error);
                print("\n");
                return;
        }

        print ("%d x %d\n", video_codec_context->width, video_codec_context->height);
        UU_MOVIE_PLAYERS_TRACE_STRING(movie.url, url_zstr);
        UU_MOVIE_PLAYERS_TRACE_VALUE(video.pix_fmt, video_codec_context->pix_fmt);

        auto output_format = FFMPEG::AV_PIX_FMT_RGBA;
        print("input pixel format: %d\n", video_codec_context->pix_fmt);
        if (!FFMPEG::sws_isSupportedInput(video_codec_context->pix_fmt)) {
                print("sws does not support this pixel format: %d\n",
                      video_codec_context->pix_fmt);
                return;
        }
        if (!FFMPEG::sws_isSupportedOutput(output_format)) {
                print("sws does not support RGBA\n");
        }
        auto sws_context =
                FFMPEG::sws_getContext(video_codec_context->width,
                                       video_codec_context->height,
                                       video_codec_context->pix_fmt,
                                       video_codec_context->width,
                                       video_codec_context->height,
                                       output_format,
                                       0,
                                       nullptr,
                                       nullptr,
                                       nullptr);
        if (!sws_context) {
                print("could not allocate swscale context\n");
                return;
        }


        {
                details::source source = {};
                source.av.underlying.format_context = format_context;
                source.av.underlying.decoding_context = video_codec_context;
                source.av.underlying.conversion_context = sws_context;
                source.av.underlying.stream_index = video_stream_index;
                source.output_format = output_format;
                queue->queue->sources.emplace_back(std::move(source));
                video_codec_context = nullptr;
                format_context = nullptr;
        }
}

uu_movie_players_api
bool decode_step(Queue* queue_container,
                 uint8_t* result_arena,
                 size_t result_arena_size, Frame* output)
{
        // TODO(nicolas):
        auto const print = log_formatted;
        arena arena = { { result_arena, 0 }, result_arena_size };
        auto& queue = *(queue_container->queue);
        struct image {
                uint8_t* data;
                size_t width;
                size_t height;
        };

        struct image_pointers {
                uint8_t** dst;
                int* dst_stride;
        };

        auto allocate_image = [&](details::source& source) -> image {
                size_t width = source.av.get().decoding_context->width;
                size_t height = source.av.get().decoding_context->height;
                auto range = push_bytes(&arena, 4*width*height);
                return image{ range.start, width, height };
        };

        auto allocate_image_pointers = [&](image image) -> image_pointers {
                uint8_t** dst;
                int* dst_stride;
                dst = reinterpret_cast<uint8_t**>(push_bytes(&arena, sizeof(*dst) * image.height).start);
                dst_stride = reinterpret_cast<int*>(push_bytes(&arena, sizeof(*dst_stride) * image.height).start);
                dst[0] = image.data;
                int stride = image.width * 4;
                dst_stride[0] = stride;
                for (size_t row_index = 1; row_index < image.height; ++row_index)
                {
                        dst[row_index] = dst[row_index - 1] + stride;
                        dst_stride[row_index] = stride;
                }
                return image_pointers{ dst, dst_stride };
        };

        auto reset_arena = [&]() {
                arena.range.size = 0;
        };

        details::source* source;
        bool next_movie = false;
        double frame_timestamp_origin = 0.0;
        do {
                if (next_movie) {
                        ++queue.source_index;
                        frame_timestamp_origin = 0.0;
                }

                if (queue.source_index >= queue.sources.size()) {
                        return false;
                }

                reset_arena();
                source = &queue.sources[queue.source_index];
                auto format_context = source->av.get().format_context;
                auto decoding_context = source->av.get().decoding_context;
                auto conversion_context = source->av.get().conversion_context;
                auto stream_index = source->av.get().stream_index;

                FFMPEG::AVPacket packet = {};
                const auto av_read_error = FFMPEG::av_read_frame
                                           (format_context, &packet);
                auto cleanup_packet = details::defer([&]() {
                        FFMPEG::av_packet_unref(&packet);

                });
                UU_MOVIE_PLAYERS_TRACE_VALUE(av_read_frame, av_read_error);
                if (av_read_error < 0) {
                        if (AVERROR_EOF != av_read_error) {
                                print("error in av_read_frame: ");
                                log_averror(av_read_error);
                                print("\n");
                        }

                        // when looping we seek back to the beginning
                        auto seek_error =
                                FFMPEG::av_seek_frame(format_context,
                                                      -1,
                                                      0,
                                                      AVSEEK_FLAG_BACKWARD);
                        if (seek_error < 0) {
                                print("error in av_seek_frame: ");
                                log_averror(seek_error);
                                print("\n");
                                next_movie = true;
                        }
                        // TODO(nicolas): this is off by one frame
                        source->frame_origin_ts_micros = source->frame_last_ts_micros;
                        UU_MOVIE_PLAYERS_TRACE_VALUE(seek, 1);
                        continue;
                }
                if (packet.stream_index != stream_index) {
                        continue;
                }
                UU_MOVIE_PLAYERS_TRACE_VALUE(packet_stream_index, packet.stream_index);
                UU_MOVIE_PLAYERS_TRACE_VALUE(packet_stream_pos, packet.pos);
                UU_MOVIE_PLAYERS_TRACE_VALUE(packet_stream_pts, packet.pts);
                UU_MOVIE_PLAYERS_TRACE_VALUE(packet_stream_dts, packet.dts);
                const auto codec_packet_error = FFMPEG::avcodec_send_packet(decoding_context,
                                                &packet);
                UU_MOVIE_PLAYERS_TRACE_VALUE(FFMPEG::avcodec_send_packet(),
                                             codec_packet_error);
                if (codec_packet_error < 0) {
                        print("error in avcodec_send_packet: ");
                        log_averror(codec_packet_error);
                        print("\n");
                        continue;
                }

                {
                        FFMPEG::AVFrame video_frame = {};
                        const auto codec_frame_error = FFMPEG::avcodec_receive_frame(decoding_context,
                                                       &video_frame);
                        auto cleanup_frame = details::defer([&]() {
                                if (codec_frame_error == 0) {
                                        av_frame_unref(&video_frame);
                                }
                        });
                        if (codec_frame_error == AVERROR_EOF) {
                                next_movie = true;
                                continue;
                        } else if (codec_frame_error < 0) {
                                print("error in avcodec_receive_frame: ");
                                log_averror(codec_frame_error);
                                print("\n");
                                continue;
                        }

                        auto const stream = format_context->streams[stream_index];
                        auto guessed_aspect_ratio =
                                FFMPEG::av_guess_sample_aspect_ratio(
                                        format_context,
                                        stream,
                                        &video_frame);
                        auto guessed_timestamp = FFMPEG::av_frame_get_best_effort_timestamp(
                                                         &video_frame);
                        UU_MOVIE_PLAYERS_TRACE_VALUE(stream.time_base_num, stream->time_base.num);
                        UU_MOVIE_PLAYERS_TRACE_VALUE(stream.time_base_den, stream->time_base.den);
                        UU_MOVIE_PLAYERS_TRACE_VALUE(context.time_base_num,
                                                     decoding_context->time_base.num);
                        UU_MOVIE_PLAYERS_TRACE_VALUE(context.time_base_den,
                                                     decoding_context->time_base.den);
                        UU_MOVIE_PLAYERS_TRACE_VALUE(aspect_ratio_num, guessed_aspect_ratio.num);
                        UU_MOVIE_PLAYERS_TRACE_VALUE(aspect_ratio_den, guessed_aspect_ratio.den);
                        UU_MOVIE_PLAYERS_TRACE_VALUE(frame.width, video_frame.width);
                        UU_MOVIE_PLAYERS_TRACE_VALUE(frame.height, video_frame.height);
                        UU_MOVIE_PLAYERS_TRACE_VALUE(frame.pts, video_frame.pts);
                        UU_MOVIE_PLAYERS_TRACE_VALUE(frame.pixel_format, video_frame.format);
                        UU_MOVIE_PLAYERS_TRACE_VALUE(frame.hw_frames_ctx,
                                                     intptr_t(video_frame.hw_frames_ctx));
                        UU_MOVIE_PLAYERS_TRACE_VALUE(frame.best_effort_timestamp,
                                                     guessed_timestamp);

                        double ms = 1000.0 * guessed_timestamp * FFMPEG::av_q2d(stream->time_base);
                        UU_MOVIE_PLAYERS_TRACE_VALUE(frame.ms, ms);
                        output->ts_micros = ms*1000;
                        output->ts_micros += source->frame_origin_ts_micros;
                        source->frame_last_ts_micros = output->ts_micros;

                        if (video_frame.format == FFMPEG::AV_PIX_FMT_YUV420P) {
                                UU_MOVIE_PLAYERS_TRACE_STRING(pixfmt, "AV_PIX_FMT_YUV420P");
                        }

                        auto image = allocate_image(*source);
                        auto image_pointers = allocate_image_pointers(image);
                        FFMPEG::sws_scale(conversion_context,
                                          video_frame.data,
                                          video_frame.linesize,
                                          0,
                                          image.height,
                                          image_pointers.dst,
                                          image_pointers.dst_stride);
                        output->data = image.data;
                        output->width = uint16_t(image.width);
                        output->height = uint16_t(image.height);
                        if (guessed_aspect_ratio.num
                            && guessed_aspect_ratio.den) {
                                output->aspect_ratio_numerator = guessed_aspect_ratio.num;
                                output->aspect_ratio_denominator = guessed_aspect_ratio.den;
                        } else {
                                // 1:1 square pixels if pixel ratio unknown
                                output->aspect_ratio_numerator = 1;
                                output->aspect_ratio_denominator = 1;
                        }
                        return true;
                }
        } while (true);
}

}
#endif
