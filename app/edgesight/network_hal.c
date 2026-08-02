/****************************************************************************
 * app/edgesight/network_hal.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to you under the Apache License, Version
 * 2.0 (the "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.  See the License for the specific language governing
 * permissions and limitations under the License.
 *
 * EdgeSight - Network HAL implementation.
 *
 * Publishes fall-detection alerts to an MQTT broker over TCP using the
 * in-tree MQTT-C client (apps/netutils/mqttc, CONFIG_NETUTILS_MQTTC).
 * The broker connection is optional: if it cannot be reached the HAL
 * degrades gracefully and the rest of the application keeps running.
 *
 * RTSP live streaming and OTA update are out of scope here: RTSP depends
 * on the H.264 encode core (still a vendor-library stub in
 * stm32n6_venc.c) and OTA is a separate flash/bootloader subsystem.
 * Both are marked TODO below.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "network_hal.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <syslog.h>

#include <mqtt.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define NET_MQTT_SNDBUF_SZ   1024
#define NET_MQTT_RCVBUF_SZ   1024
#define NET_MQTT_KEEPALIVE   60      /* seconds */
#define NET_ALERT_JSON_SZ    256

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* Backing store for the MQTT client.  ctx->mqtt_client points here so the
 * public header stays free of the MQTT-C include.
 */

struct network_mqtt_s
{
  struct mqtt_client client;
  int                sockfd;
  uint8_t            sndbuf[NET_MQTT_SNDBUF_SZ];
  uint8_t            rcvbuf[NET_MQTT_RCVBUF_SZ];
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct network_mqtt_s g_network_mqtt;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: network_hal_publish_cb
 *
 * Description:
 *   MQTT-C publish-response callback.  This client only publishes, so
 *   inbound messages are ignored.
 *
 ****************************************************************************/

static void network_hal_publish_cb(FAR void **state,
                                    FAR struct mqtt_response_publish *pub)
{
  UNUSED(state);
  UNUSED(pub);
}

/****************************************************************************
 * Name: network_hal_tcp_connect
 *
 * Description:
 *   Open a blocking TCP connection to host:port.  Returns a socket fd on
 *   success or a negated errno on failure.
 *
 ****************************************************************************/

static int network_hal_tcp_connect(FAR const char *host, uint16_t port)
{
  struct sockaddr_in addr;
  int fd;

  fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    {
      return -errno;
    }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port   = htons(port);

  if (inet_pton(AF_INET, host, &addr.sin_addr) != 1)
    {
      syslog(LOG_ERR, "network: bad broker address '%s'\n", host);
      close(fd);
      return -EINVAL;
    }

  if (connect(fd, (FAR struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
      int err = errno;
      close(fd);
      return -err;
    }

  return fd;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int network_hal_init(struct network_context_s *ctx,
                     const struct network_config_s *cfg)
{
  memset(ctx, 0, sizeof(*ctx));
  ctx->config = *cfg;

  memset(&g_network_mqtt, 0, sizeof(g_network_mqtt));
  g_network_mqtt.sockfd = -1;
  ctx->mqtt_client = &g_network_mqtt;

  ctx->state = NET_STATE_DISCONNECTED;
  ctx->initialized = true;

  syslog(LOG_INFO, "network: initialized, broker=%s:%u\n",
         cfg->mqtt_broker ? cfg->mqtt_broker : "none",
         cfg->mqtt_port);
  return 0;
}

int network_hal_connect(struct network_context_s *ctx)
{
  FAR struct network_mqtt_s *nm;
  int mqtterr;
  uint8_t flags;

  if (!ctx->initialized)
    {
      return -1;
    }

  if (ctx->config.mqtt_broker == NULL)
    {
      syslog(LOG_WARNING, "network: no broker configured\n");
      return -EINVAL;
    }

  nm = (FAR struct network_mqtt_s *)ctx->mqtt_client;
  ctx->state = NET_STATE_CONNECTING;

  /* Establish the TCP transport to the broker. */

  nm->sockfd = network_hal_tcp_connect(ctx->config.mqtt_broker,
                                       ctx->config.mqtt_port);
  if (nm->sockfd < 0)
    {
      syslog(LOG_WARNING, "network: broker %s:%u unreachable (%d)\n",
             ctx->config.mqtt_broker, ctx->config.mqtt_port,
             nm->sockfd);
      ctx->state = NET_STATE_ERROR;
      return nm->sockfd;
    }

  /* Bind the MQTT client to the socket and issue CONNECT. */

  mqtt_init(&nm->client, nm->sockfd,
            nm->sndbuf, sizeof(nm->sndbuf),
            nm->rcvbuf, sizeof(nm->rcvbuf),
            network_hal_publish_cb);

  flags = MQTT_CONNECT_CLEAN_SESSION;
  mqtterr = mqtt_connect(&nm->client,
                         ctx->config.mqtt_client_id,
                         NULL, NULL, 0, NULL, NULL,
                         flags, NET_MQTT_KEEPALIVE);
  if (mqtterr != MQTT_OK)
    {
      syslog(LOG_ERR, "network: mqtt_connect failed: %s\n",
             mqtt_error_str(mqtterr));
      close(nm->sockfd);
      nm->sockfd = -1;
      ctx->state = NET_STATE_ERROR;
      return -EIO;
    }

  /* Drive the CONNECT out onto the wire. */

  mqtt_sync(&nm->client);

  ctx->state = NET_STATE_CONNECTED;
  syslog(LOG_INFO, "network: connected to broker %s:%u\n",
         ctx->config.mqtt_broker, ctx->config.mqtt_port);
  return 0;
}

int network_hal_send_alert(struct network_context_s *ctx,
                           const struct alert_message_s *alert)
{
  FAR struct network_mqtt_s *nm;
  int mqtterr;
  char payload[NET_ALERT_JSON_SZ];
  const char *topic;
  int len;

  if (!ctx->initialized || ctx->state != NET_STATE_CONNECTED)
    {
      ctx->alerts_failed++;
      return -1;
    }

  nm = (FAR struct network_mqtt_s *)ctx->mqtt_client;

  /* Format the alert as a compact JSON document. */

  len = snprintf(payload, sizeof(payload),
                 "{\"level\":%u,\"confidence\":%.2f,"
                 "\"angle\":%.1f,\"frame\":%lu,\"msg\":\"%s\"}",
                 alert->level,
                 (double)alert->confidence,
                 (double)alert->torso_angle,
                 (unsigned long)alert->frame_number,
                 alert->description ? alert->description : "");

  if (len < 0)
    {
      ctx->alerts_failed++;
      return -EINVAL;
    }

  topic = ctx->config.mqtt_topic ?
          ctx->config.mqtt_topic : "edgesight/alerts";

  mqtterr = mqtt_publish(&nm->client, topic, payload, (size_t)len,
                         MQTT_PUBLISH_QOS_1);
  if (mqtterr != MQTT_OK)
    {
      syslog(LOG_ERR, "network: mqtt_publish failed: %s\n",
             mqtt_error_str(mqtterr));
      ctx->alerts_failed++;
      return -EIO;
    }

  /* Flush the PUBLISH to the broker. */

  if (mqtt_sync(&nm->client) != MQTT_OK)
    {
      ctx->alerts_failed++;
      return -EIO;
    }

  ctx->alerts_sent++;
  syslog(LOG_INFO, "network: alert published to %s (level=%u)\n",
         topic, alert->level);
  return 0;
}

int network_hal_poll(struct network_context_s *ctx)
{
  FAR struct network_mqtt_s *nm;

  if (!ctx->initialized)
    {
      return NET_STATE_DISCONNECTED;
    }

  /* Service the MQTT client: sends pending packets, processes keep-alive
   * pings and any broker responses.  A transport error drops us back to
   * the error state so the caller can retry network_hal_connect().
   */

  if (ctx->state == NET_STATE_CONNECTED)
    {
      nm = (FAR struct network_mqtt_s *)ctx->mqtt_client;
      if (mqtt_sync(&nm->client) != MQTT_OK)
        {
          ctx->state = NET_STATE_ERROR;
        }
    }

  /* TODO: RTSP live streaming and OTA update.  RTSP needs the H.264
   * encode core (stm32n6_venc.c vendor stub); OTA is a separate flash
   * partition / bootloader subsystem.
   */

  return (int)ctx->state;
}

uint32_t network_hal_get_state(const struct network_context_s *ctx)
{
  return ctx->state;
}

void network_hal_deinit(struct network_context_s *ctx)
{
  FAR struct network_mqtt_s *nm;

  if (!ctx->initialized)
    {
      return;
    }

  nm = (FAR struct network_mqtt_s *)ctx->mqtt_client;
  if (nm != NULL && nm->sockfd >= 0)
    {
      mqtt_disconnect(&nm->client);
      mqtt_sync(&nm->client);
      close(nm->sockfd);
      nm->sockfd = -1;
    }

  memset(ctx, 0, sizeof(*ctx));
  syslog(LOG_INFO, "network: deinitialized\n");
}
