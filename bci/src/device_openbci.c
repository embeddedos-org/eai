// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_bci/device.h"
#include "eai/log.h"
#include <string.h>
#include <stdlib.h>

#ifdef EAI_BCI_OPENBCI_ENABLED

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>

#define OPENBCI_PACKET_SIZE  33   /* 1 header + 8ch*3bytes + 3 accel + 1 footer */
#define OPENBCI_HEADER_BYTE  0xA0
#define OPENBCI_FOOTER_BYTE  0xC0
#define OPENBCI_NUM_CHANNELS 8
#define OPENBCI_SCALE_UV     (187.5f / 8388607.0f)  /* 24-bit ADC to uV */

typedef struct {
    char     port[64];
    uint32_t baud_rate;
    uint8_t  board_type;
    bool     streaming;
    int      fd;         /* serial port file descriptor */
} openbci_ctx_t;

static openbci_ctx_t g_openbci_ctx;

/* Convert baud rate to termios constant */
static speed_t baud_to_speed(uint32_t baud) {
    switch (baud) {
        case 9600:   return B9600;
        case 19200:  return B19200;
        case 38400:  return B38400;
        case 57600:  return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
        default:     return B115200;
    }
}

/* Open and configure serial port */
static int openbci_serial_open(openbci_ctx_t *ctx) {
    ctx->fd = open(ctx->port, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (ctx->fd < 0) {
        EAI_LOG_ERROR("bci", "OpenBCI: failed to open %s: %s", ctx->port, strerror(errno));
        return -1;
    }

    /* Clear non-blocking after open */
    int flags = fcntl(ctx->fd, F_GETFL, 0);
    fcntl(ctx->fd, F_SETFL, flags & ~O_NONBLOCK);

    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(ctx->fd, &tty) != 0) {
        EAI_LOG_ERROR("bci", "OpenBCI: tcgetattr failed: %s", strerror(errno));
        close(ctx->fd);
        ctx->fd = -1;
        return -1;
    }

    speed_t spd = baud_to_speed(ctx->baud_rate);
    cfsetispeed(&tty, spd);
    cfsetospeed(&tty, spd);

    /* 8N1, no flow control */
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CREAD | CLOCAL;

    /* Raw mode */
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL | INLCR);
    tty.c_oflag &= ~OPOST;

    /* Read timeout: 100ms */
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(ctx->fd, TCSANOW, &tty) != 0) {
        EAI_LOG_ERROR("bci", "OpenBCI: tcsetattr failed: %s", strerror(errno));
        close(ctx->fd);
        ctx->fd = -1;
        return -1;
    }

    tcflush(ctx->fd, TCIOFLUSH);
    EAI_LOG_INFO("bci", "OpenBCI: serial port %s opened @ %u baud", ctx->port, ctx->baud_rate);
    return 0;
}

/* Send a command string to the board */
static int openbci_send_cmd(openbci_ctx_t *ctx, const char *cmd) {
    if (ctx->fd < 0) return -1;
    ssize_t n = write(ctx->fd, cmd, strlen(cmd));
    if (n < 0) {
        EAI_LOG_ERROR("bci", "OpenBCI: write failed: %s", strerror(errno));
        return -1;
    }
    /* Small delay for board to process */
    usleep(100000);
    return 0;
}

/* Decode a 24-bit signed big-endian sample */
static int32_t decode_24bit_sample(const uint8_t *p) {
    int32_t raw = ((int32_t)p[0] << 16) | ((int32_t)p[1] << 8) | (int32_t)p[2];
    /* Sign extend from 24-bit */
    if (raw & 0x800000) {
        raw |= (int32_t)0xFF000000;
    }
    return raw;
}

/* Read exactly n bytes from serial, with retry */
static int serial_read_exact(int fd, uint8_t *buf, size_t n, int timeout_ms) {
    size_t total = 0;
    int elapsed = 0;
    while (total < n && elapsed < timeout_ms) {
        ssize_t r = read(fd, buf + total, n - total);
        if (r > 0) {
            total += (size_t)r;
        } else if (r == 0) {
            usleep(1000);
            elapsed++;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(1000);
                elapsed++;
            } else {
                return -1;
            }
        }
    }
    return (total == n) ? 0 : -1;
}

static eai_status_t openbci_init(eai_bci_device_t *dev, const eai_kv_t *params, int param_count)
{
    memset(&g_openbci_ctx, 0, sizeof(g_openbci_ctx));
    strncpy(g_openbci_ctx.port, "/dev/ttyUSB0", sizeof(g_openbci_ctx.port) - 1);
    g_openbci_ctx.baud_rate  = 115200;
    g_openbci_ctx.board_type = 0;
    g_openbci_ctx.fd         = -1;

    for (int i = 0; i < param_count && params; i++) {
        if (strcmp(params[i].key, "port") == 0)
            strncpy(g_openbci_ctx.port, params[i].value, sizeof(g_openbci_ctx.port) - 1);
        else if (strcmp(params[i].key, "baud_rate") == 0)
            g_openbci_ctx.baud_rate = (uint32_t)atoi(params[i].value);
    }

    dev->ctx            = &g_openbci_ctx;
    dev->num_channels   = OPENBCI_NUM_CHANNELS;
    dev->sample_rate_hz = 250;
    dev->state          = EAI_BCI_STATE_DISCONNECTED;

    EAI_LOG_INFO("bci", "OpenBCI: init on %s @ %u baud", g_openbci_ctx.port, g_openbci_ctx.baud_rate);
    return EAI_OK;
}

static eai_status_t openbci_start(eai_bci_device_t *dev)
{
    openbci_ctx_t *ctx = (openbci_ctx_t *)dev->ctx;

    /* Open serial port if not already open */
    if (ctx->fd < 0) {
        if (openbci_serial_open(ctx) != 0) {
            dev->state = EAI_BCI_STATE_DISCONNECTED;
            return EAI_ERR_BCI_SIGNAL;
        }
    }

    /* Send 'b' to start streaming (OpenBCI protocol) */
    if (openbci_send_cmd(ctx, "b") != 0) {
        EAI_LOG_ERROR("bci", "OpenBCI: failed to send start command");
        return EAI_ERR_BCI_SIGNAL;
    }

    ctx->streaming = true;
    dev->state = EAI_BCI_STATE_STREAMING;
    EAI_LOG_INFO("bci", "OpenBCI: streaming started on %s", ctx->port);
    return EAI_OK;
}

static eai_status_t openbci_stop(eai_bci_device_t *dev)
{
    openbci_ctx_t *ctx = (openbci_ctx_t *)dev->ctx;

    /* Send 's' to stop streaming (OpenBCI protocol) */
    if (ctx->fd >= 0) {
        openbci_send_cmd(ctx, "s");
        close(ctx->fd);
        ctx->fd = -1;
    }

    ctx->streaming = false;
    dev->state = EAI_BCI_STATE_DISCONNECTED;
    EAI_LOG_INFO("bci", "OpenBCI: streaming stopped, port closed");
    return EAI_OK;
}

static eai_status_t openbci_read(eai_bci_device_t *dev, eai_bci_sample_t *out,
                                  int max_samples, int *samples_read)
{
    openbci_ctx_t *ctx = (openbci_ctx_t *)dev->ctx;
    if (!ctx->streaming || ctx->fd < 0) return EAI_ERR_BCI_SIGNAL;

    int count = 0;
    uint8_t packet[OPENBCI_PACKET_SIZE];

    while (count < max_samples) {
        /* Sync: find header byte 0xA0 */
        uint8_t byte;
        int synced = 0;
        for (int attempt = 0; attempt < 256; attempt++) {
            ssize_t r = read(ctx->fd, &byte, 1);
            if (r != 1) break;
            if (byte == OPENBCI_HEADER_BYTE) {
                synced = 1;
                break;
            }
        }
        if (!synced) break;

        /* Read remaining 32 bytes of the packet */
        packet[0] = OPENBCI_HEADER_BYTE;
        if (serial_read_exact(ctx->fd, packet + 1, OPENBCI_PACKET_SIZE - 1, 100) != 0) {
            break;
        }

        /* Verify footer byte */
        if (packet[OPENBCI_PACKET_SIZE - 1] != OPENBCI_FOOTER_BYTE) {
            EAI_LOG_WARN("bci", "OpenBCI: bad footer 0x%02x, skipping", packet[OPENBCI_PACKET_SIZE - 1]);
            continue;
        }

        /* Decode 8 channels of 24-bit samples (bytes 2-25) */
        eai_bci_sample_t *sample = &out[count];
        memset(sample, 0, sizeof(*sample));
        sample->quality = 100;

        for (int ch = 0; ch < OPENBCI_NUM_CHANNELS; ch++) {
            int offset = 2 + ch * 3;  /* skip header + sample number */
            int32_t raw = decode_24bit_sample(&packet[offset]);
            sample->channels[ch] = (float)raw * OPENBCI_SCALE_UV;
        }

        count++;
    }

    if (samples_read) *samples_read = count;
    return (count > 0) ? EAI_OK : EAI_ERR_BCI_SIGNAL;
}

static eai_status_t openbci_calibrate(eai_bci_device_t *dev)
{
    openbci_ctx_t *ctx = (openbci_ctx_t *)dev->ctx;
    dev->state = EAI_BCI_STATE_CALIBRATING;

    /* Open port if needed */
    if (ctx->fd < 0) {
        if (openbci_serial_open(ctx) != 0) {
            dev->state = EAI_BCI_STATE_DISCONNECTED;
            return EAI_ERR_BCI_SIGNAL;
        }
    }

    /*
     * Send calibration test signal command per OpenBCI protocol:
     * 'x1060110X' — Channel 1, power on, gain 24x, SRB2 on, normal input
     * This configures channel 1 for a known test signal to verify ADC operation.
     */
    if (openbci_send_cmd(ctx, "x1060110X") != 0) {
        EAI_LOG_ERROR("bci", "OpenBCI: calibration command failed");
        dev->state = EAI_BCI_STATE_DISCONNECTED;
        return EAI_ERR_BCI_SIGNAL;
    }

    EAI_LOG_INFO("bci", "OpenBCI: calibration test signal enabled");
    dev->state = EAI_BCI_STATE_STREAMING;
    return EAI_OK;
}

static eai_status_t openbci_impedance(eai_bci_device_t *dev, float *ohms, int max_ch)
{
    openbci_ctx_t *ctx = (openbci_ctx_t *)dev->ctx;

    /* Open port if needed */
    if (ctx->fd < 0) {
        if (openbci_serial_open(ctx) != 0) {
            return EAI_ERR_BCI_SIGNAL;
        }
    }

    int n = max_ch > OPENBCI_NUM_CHANNELS ? OPENBCI_NUM_CHANNELS : max_ch;

    /*
     * Enable impedance measurement per OpenBCI protocol:
     * 'z' followed by channel config commands.
     * For each channel, send 'z<ch>1Z' to enable impedance on P-input.
     */
    const char *imp_cmds[] = {
        "z110Z", "z210Z", "z310Z", "z410Z",
        "z510Z", "z610Z", "z710Z", "z810Z"
    };

    for (int ch = 0; ch < n; ch++) {
        if (openbci_send_cmd(ctx, imp_cmds[ch]) != 0) {
            EAI_LOG_WARN("bci", "OpenBCI: impedance cmd failed for ch %d", ch + 1);
            ohms[ch] = -1.0f;
            continue;
        }

        /* Read a few packets and estimate impedance from signal amplitude */
        uint8_t packet[OPENBCI_PACKET_SIZE];
        float sum = 0.0f;
        int valid = 0;

        /* Start brief streaming to capture impedance data */
        openbci_send_cmd(ctx, "b");
        usleep(500000); /* 500ms of data */

        for (int s = 0; s < 125; s++) {  /* ~500ms at 250Hz */
            uint8_t byte;
            ssize_t r = read(ctx->fd, &byte, 1);
            if (r != 1 || byte != OPENBCI_HEADER_BYTE) continue;

            packet[0] = OPENBCI_HEADER_BYTE;
            if (serial_read_exact(ctx->fd, packet + 1, OPENBCI_PACKET_SIZE - 1, 50) != 0)
                continue;
            if (packet[OPENBCI_PACKET_SIZE - 1] != OPENBCI_FOOTER_BYTE)
                continue;

            int32_t raw = decode_24bit_sample(&packet[2 + ch * 3]);
            float uv = (float)raw * OPENBCI_SCALE_UV;
            if (uv < 0) uv = -uv;
            sum += uv;
            valid++;
        }

        openbci_send_cmd(ctx, "s");

        if (valid > 0) {
            /*
             * Rough impedance estimate: Z = V_rms / I_excitation
             * OpenBCI uses 6nA excitation current at 31.25Hz.
             * Z (ohms) ~ V_rms(uV) / 6e-3
             */
            float avg_uv = sum / (float)valid;
            ohms[ch] = (avg_uv / 0.006f);
        } else {
            ohms[ch] = -1.0f;
        }

        /* Disable impedance on this channel */
        const char *off_cmds[] = {
            "z100Z", "z200Z", "z300Z", "z400Z",
            "z500Z", "z600Z", "z700Z", "z800Z"
        };
        openbci_send_cmd(ctx, off_cmds[ch]);
    }

    EAI_LOG_INFO("bci", "OpenBCI: impedance measurement complete for %d channels", n);
    return EAI_OK;
}

static void openbci_shutdown(eai_bci_device_t *dev)
{
    openbci_ctx_t *ctx = (openbci_ctx_t *)dev->ctx;
    if (ctx->fd >= 0) {
        openbci_send_cmd(ctx, "s");  /* stop streaming */
        close(ctx->fd);
        ctx->fd = -1;
    }
    ctx->streaming = false;
    dev->state = EAI_BCI_STATE_DISCONNECTED;
    dev->ctx = NULL;
}

const eai_bci_device_ops_t eai_bci_device_openbci_ops = {
    .name          = "openbci",
    .type          = EAI_BCI_DEV_OPENBCI,
    .init          = openbci_init,
    .start_stream  = openbci_start,
    .stop_stream   = openbci_stop,
    .read_samples  = openbci_read,
    .calibrate     = openbci_calibrate,
    .get_impedance = openbci_impedance,
    .shutdown      = openbci_shutdown,
};

#endif /* EAI_BCI_OPENBCI_ENABLED */
