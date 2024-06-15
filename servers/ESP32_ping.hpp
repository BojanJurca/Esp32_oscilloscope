/*

    ESP32_ping.hpp

    This file is part of Multitasking Esp32 ping class: https://github.com/BojanJurca/Multitasking-Esp32-ping-class

    The librari is based on Jaume Olivé's work: https://github.com/pbecchi/ESP32_ping
    and D. Varrels's work, which is included in Arduino library list: https://github.com/dvarrel

    Both libraries mentioned above do not support the following use-cases, that are needed in Multitasking Esp32 HTTP FTP Telnet servers for Arduino project:
      - multitasking
      - intermediate results display
    This is the reason for creating ESP32_ping.hpp


    May 22, 2024, Bojan Jurca
    
*/


#ifndef __ESP32_ping_HPP__
    #define __ESP32_ping_HPP__

    #include <WiFi.h>
    #include <lwip/netdb.h>


    #ifndef PING_DEFAULT_COUNT
        #define PING_DEFAULT_COUNT     10
    #endif
    #ifndef PING_DEFAULT_INTERVAL
        #define PING_DEFAULT_INTERVAL  1
    #endif
    #ifndef PING_DEFAULT_SIZE
        #define PING_DEFAULT_SIZE     32
    #endif
    #ifndef PING_DEFAULT_TIMEOUT
        #define PING_DEFAULT_TIMEOUT   1
    #endif

    #include <lwip/netdb.h>
    #include "lwip/inet_chksum.h"
    #include "lwip/ip.h"
    #include "lwip/icmp.h"

    #define EAGAIN 11
    #define ENAVAIL 119


    // Data structure that keeps reply information form different (simultaneous) pings
    struct {
        uint16_t seqno;                 // packet sequence number
        unsigned long __elapsed_time__; // in microseconds
    } pingReplies [MEMP_NUM_NETCONN];   // there can only be as much replies as there are sockets available


    class esp32_ping {

        public:

            esp32_ping () {}
            esp32_ping (const char *pingTarget) {
                // Resolve name            
                hostent *he = gethostbyname (pingTarget);
                if (he == NULL || he->h_length == 0) {
                    __errno__ = h_errno;
                    return;
                }

                __target__.addr = *(in_addr_t *) he->h_addr;
            }
            esp32_ping (IPAddress pingTarget) : esp32_ping (pingTarget.toString ().c_str ()) {}

            err_t ping (const char *pingTarget, int count = PING_DEFAULT_COUNT, int interval = PING_DEFAULT_INTERVAL, int size = PING_DEFAULT_SIZE, int timeout = PING_DEFAULT_TIMEOUT) {
                __errno__ = 0; // Clear error code

                // Resolve name            
                hostent *he = gethostbyname (pingTarget);
                if (he == NULL || he->h_length == 0)
                    return __errno__ = h_errno;

                __target__.addr = *(in_addr_t *) he->h_addr;

                __size__ = size;

                return ping (count, interval, size, timeout);
            }

            err_t ping (IPAddress target, int count = PING_DEFAULT_COUNT, int interval = PING_DEFAULT_INTERVAL, int size = PING_DEFAULT_SIZE, int timeout = PING_DEFAULT_TIMEOUT) {
                return ping (target.toString ().c_str(), count, interval, size, timeout);
            }

            err_t ping (int count = PING_DEFAULT_COUNT, int interval = PING_DEFAULT_INTERVAL, int size = PING_DEFAULT_SIZE, int timeout = PING_DEFAULT_TIMEOUT) {

                // Check argument values
                if (count < 0) return ERR_VAL; // note that count = 0 is valid argument value, meaning infinite pinging
                if (interval < 1 || interval > 3600) return ERR_VAL; 
                if (size < 4 || size > 1024) return ERR_VAL; // max ping size is 65535 but ESP32 can't handle it
                if (timeout < 1 || timeout > 30) return ERR_VAL;

                // Initialize measuring variables
                __sent__ = 0; 
                __received__ = 0;
                __lost__ = 0;
                __stopped__ = false;
                __min_time__ = 1.E+9; // FLT_MAX;
                __max_time__ = 0.0;
                __mean_time__ = 0.0;
                __var_time__ = 0.0;

                // Create socket
                int s;
                if ((s = socket (AF_INET, SOCK_RAW, IP_PROTO_ICMP)) < 0)
                    return __errno__ = errno;

                // Make the socket non-blocking, so we can detect time-out later     
                if (fcntl (s, F_SETFL, O_NONBLOCK) == -1) {
                    close (s);
                    return __errno__ = errno;
                }

                // Begin ping ...
                char ipa [INET_ADDRSTRLEN]; 

                strcpy (ipa, inet_ntoa (__target__));
                log_i ("PING %s: %d data bytes", ipa, size);
                                
                while ((__seqno__ < count || count == 0) && (!__stopped__)) {
                    unsigned long sendMillis = millis ();

                    err_t err = __ping_send__ (s, &__target__, size);
                    if (err == ERR_OK) {
                        __sent__ ++;
                        int bytesReceived;
                        __ping_recv__ (s, &bytesReceived, 1000000 * timeout);
                        if (pingReplies [s - LWIP_SOCKET_OFFSET].__elapsed_time__) { // > 0, meaning that echo reply packet has been received
                            __received__++;

                            // Update statistics
                            __elapsed_time__ = (float) pingReplies [s - LWIP_SOCKET_OFFSET].__elapsed_time__ / (float) 1000;
                            // Mean and variance are computed in an incremental way
                            if (__elapsed_time__ < __min_time__)
                                __min_time__ = __elapsed_time__;

                            if (__elapsed_time__ > __max_time__)
                                __max_time__ = __elapsed_time__;

                            __last_mean_time__ = __mean_time__;
                            __mean_time__ = (((__received__ - 1) * __mean_time__) + __elapsed_time__) / __received__;

                            if (__received__ > 1)
                                __var_time__ = __var_time__ + ((__elapsed_time__ - __last_mean_time__) * (__elapsed_time__ - __mean_time__));

                            // Print ...
                            log_d ("%d bytes from %s: icmp_seq=%d time=%.3f ms", len, ipa, ntohs(iecho->seqno), __elapsed_time__);

                        } else {
                            __lost__ ++;
                            __elapsed_time__ = 0;
                        }

                        // Reporting of intermediate results 
                        onReceive (bytesReceived);
                    } else {
                        __errno__ = err;
                        log_e ("__ping_send__ error %i", err);
                        break;
                    }

                    if(__seqno__ < count || count == 0){ 
                        while ((millis () - sendMillis < 1000L * interval) && (!__stopped__)) {
                            // Reporting of waiting 
                            onWait ();
                            delay (10);
                        }
                    }
                }

                closesocket (s);
                return ERR_OK;
            }

            // Returns the target beeing pinged
            IPAddress target () { 
                char str [INET_ADDRSTRLEN]; 
                inet_ntop (AF_INET, &(__target__.addr), str, INET_ADDRSTRLEN); 
                IPAddress adr;
                adr.fromString (str);
                return adr;
            }

            inline int size () __attribute__((always_inline)) { return __size__; }

            // Request to stop pinging
            inline void stop () __attribute__((always_inline)) { __stopped__ = true; }

            // Statistics: mean and variance are computed in an incremental way
            inline uint32_t sent () __attribute__((always_inline)) { return __sent__; }
            inline uint32_t received () __attribute__((always_inline)) { return __received__; }
            inline uint32_t lost () __attribute__((always_inline)) { return __lost__; }
            inline float elapsed_time () __attribute__((always_inline)) { return __elapsed_time__; }
            inline float min_time () __attribute__((always_inline)) { return __min_time__; }
            inline float max_time () __attribute__((always_inline)) { return __max_time__; }
            inline float mean_time () __attribute__((always_inline)) { return __mean_time__; }
            inline float var_time () __attribute__((always_inline)) { return __var_time__; }

            // Error reporting
            inline err_t error () __attribute__((always_inline)) { return __errno__; }

            // Reporting intermediate results - override (use) these functions if needed
            virtual void onReceive (int bytes) {}
            virtual void onWait () {}


        private:

            ip4_addr_t __target__ = {};   // the target beeing pinged
            int __size__ = 0;
            uint16_t __seqno__ = 0;       // sequence number of the packet
            uint32_t __sent__ = 0;        // number of packets sent
            uint32_t __received__ = 0;    // number of packets receivd
            uint32_t __lost__ = 0;        // number of packets lost
            bool __stopped__ = false;     // request to stop pinging
            err_t __errno__ = ERR_OK;     // the last error number of socket operations

            // Statistics: mean and variance are computed in an incremental way
            float __elapsed_time__ = 0;
            float __min_time__ = 0;
            float __max_time__ = 0;
            float __mean_time__ = 0;
            float __var_time__ = 0;
            float __last_mean_time__ = 0;


            err_t __ping_send__ (int s, ip4_addr_t *addr, int size) {
                struct icmp_echo_hdr *iecho;
                struct sockaddr_in to;
                size_t ping_size = sizeof(struct icmp_echo_hdr) + size;

                // Construct ping block
                // - First there is struct icmp_echo_hdr (https://github.com/ARMmbed/lwip/blob/master/src/include/lwip/prot/icmG.h). We'll use these fields:
                //    - uint16_t id      - this is where we'll keep the socket number so that we would know from where ping packet has been send when we receive a reply 
                //    - uint16_t seqno   - each packet gets it sequence number so we can distinguish one packet from another when we receive a reply
                //    - uint16_t chksum  - needs to be calcualted
                // - Then we'll add the payload:
                //    - unsigned long micros  - this is where we'll keep the time packet has been sent so we can calcluate round-trip time when we receive a reply
                //    - unimportant data, just to fill the payload to the desired length

                iecho = (struct icmp_echo_hdr *) mem_malloc ((mem_size_t) ping_size);
                if (!iecho) {
                    log_e ("error: mem_malloc");
                    return ERR_MEM;
                }
        
                // initialize structure where the reply information will be stored when it arrives
                __seqno__ ++;
                pingReplies [s - LWIP_SOCKET_OFFSET] = { __seqno__, 0 };

                // prepare echo packet
                size_t data_len = ping_size - sizeof (struct icmp_echo_hdr);
                ICMPH_TYPE_SET (iecho, ICMP_ECHO);
                ICMPH_CODE_SET (iecho, 0);
                iecho->chksum = 0;
                iecho->id = s; // id = socket
                iecho->seqno = __seqno__; // no need to call htons(__seqno__), we'll get an echo reply back to the same machine
                // store micros at send time
                unsigned long sendMicros = micros ();
                *(unsigned long *) &((char *) iecho) [sizeof (struct icmp_echo_hdr)] = sendMicros;
                // fill the additional data buffer with some data
                int i = sizeof (sendMicros);
                for (; i < data_len; i++)
                    ((char*) iecho) [sizeof (struct icmp_echo_hdr) + i] = (char) i;
                // claculate checksum
                iecho->chksum = inet_chksum (iecho, ping_size);

                // send the packet
                to.sin_len = sizeof (to);
                to.sin_family = AF_INET;
                inet_addr_from_ip4addr (&to.sin_addr, addr);
                int sent = sendto (s, iecho, ping_size, 0, (struct sockaddr*) &to, sizeof (to));
                mem_free (iecho);
                if (sent != ping_size) {
                    log_e ("error: sendto returned %i", sent);
                    return ERR_VAL;
                } else {
                    log_i ("packet %i sent through socket %i\n", __seqno__, s);
                    return ERR_OK;
                }
            }


            err_t __ping_recv__ (int s, int *bytes, unsigned long timeoutMicros) {
                char buf [64];
                int fromlen;
                struct sockaddr_in from;
                struct ip_hdr *iphdr;
                struct icmp_echo_hdr *iecho = NULL;
                char ipa [INET_ADDRSTRLEN]; 
                float __elapsed_time__;
                float last_mean_time;

                unsigned long startMicros = micros (); // to calculate time-out

                // Receive the echo packet
                while (true) {

                    // Did some other process poick up our echo reply and already done the job for us? 
                    if (pingReplies [s - LWIP_SOCKET_OFFSET].__elapsed_time__) { // > 0
                        log_i ("Another process already did the job for socket %i, returning", s);
                        return ERR_OK;
                    }

                    // Read echo packet without waiting
                    if ((*bytes = recvfrom (s, buf, sizeof (buf), 0, (struct sockaddr*) &from, (socklen_t*) &fromlen)) <= 0) {
                        yield ();
                        if ((errno == EAGAIN || errno == ENAVAIL) && (micros () - startMicros < timeoutMicros)) continue; // not time-out yet 
                        return errno; // time-out
                    }

                    if (*bytes < (int) (sizeof(struct ip_hdr) + sizeof (struct icmp_echo_hdr))) {
                        log_i ("Echo packet received from %s is too short", ipa);
                        continue;
                    }

                    // Get from IP address
                    ip4_addr_t fromaddr;
                    inet_addr_to_ip4addr (&fromaddr, &from.sin_addr);
                    strcpy (ipa, inet_ntoa (fromaddr));

                    // Get echo
                    iphdr = (struct ip_hdr *) buf;
                    iecho = (struct icmp_echo_hdr *) (buf + (IPH_HL (iphdr) * 4));

                    if (iecho->id < LWIP_SOCKET_OFFSET || iecho->id >= LWIP_SOCKET_OFFSET + MEMP_NUM_NETCONN) {
                        log_i ("Echo packet received from %s does not have the correct id", ipa);
                        continue;
                    }

                    // Now we should consider several options:

                        // Did we pick up the echo packet that was send through the socket s?
                        if (s == iecho->id) { // we picked up the echo packet that was sent from the same socket

                            // Did we pick up the echo packet with the latest sequence number?
                            if (pingReplies [s - LWIP_SOCKET_OFFSET].seqno == iecho->seqno) {
                                log_i ("Echo packet %u received trough socket %i", iecho->seqno, s);

                                // Get send time from echo packet
                                unsigned long sentMicros = *(unsigned long *) &((char *)iecho) [sizeof (struct icmp_echo_hdr)];
                                // Write information about the reply in data structure
                                pingReplies [s - LWIP_SOCKET_OFFSET].__elapsed_time__ = micros () - sentMicros; 

                                return ERR_OK;

                            } else { // Sequence numbers do not match, ignore this echo packet, its time-out has probably already been reported
                                log_i ("Echo packet %u arrived too late (%lu us) trough socket %i", iecho->seqno, micros () - *(unsigned long *) &((char *)iecho)[sizeof(struct icmp_echo_hdr)], s);
                            }

                        } else { // we picked up an echo packet that was sent from another socket

                            log_i ("Echo packet %u, that belongs to another socket %i received through socket %i", iecho->seqno, iecho->id, s);

                            // Did we pick up the echo packet with the latest sequence number?
                            if (pingReplies [iecho->id - LWIP_SOCKET_OFFSET].seqno == iecho->seqno) {

                                // Get send time from echo packet
                                unsigned long sentMicros = *(unsigned long *) &((char *)iecho) [sizeof(struct icmp_echo_hdr)];
                                // Write information about the reply in data structure
                                pingReplies [iecho->id - LWIP_SOCKET_OFFSET].__elapsed_time__ = micros () - sentMicros; 

                                // do not return now, continue waiting for the right echo packet
 
                            } else { // Sequence numbers do not match, ignore this echo packet, its time-out has probably already been reported
                                log_i ("Echo packet %u, that belongs to another socket %i arrived too late (%lu us) through socket %i", iecho->seqno,  iecho->id, micros () - *(unsigned long *) &((char *)iecho)[sizeof(struct icmp_echo_hdr)], s);
                            }

                        }
            
                }

            }

    };

#endif