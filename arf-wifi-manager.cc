/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2004,2005,2006 INRIA
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */

#include "arf-wifi-manager.h"
#include "ns3/log.h"
#include "ns3/uinteger.h"

//Macro defined to return mininum if two numbers passed as arguments
#define Min(a,b) ((a < b) ? a : b) 

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("ArfWifiManager");

/**
 * \brief hold per-remote-station state for ARF Wifi manager.
 *
 * This struct extends from WifiRemoteStation struct to hold additional
 * information required by the ARF Wifi manager
 */
struct ArfWifiRemoteStation : public WifiRemoteStation
{
  uint32_t m_timer; ///< timer value
  uint32_t m_success; ///< success count
  uint32_t m_failed; ///< failed count
  bool m_recovery; ///< recovery
  uint32_t m_retry; ///< retry count
  uint32_t m_timerTimeout; ///< timer timeout
  uint32_t m_successThreshold; ///< success threshold
  uint32_t m_rate; ///< rate
};

NS_OBJECT_ENSURE_REGISTERED (ArfWifiManager);
/**/
TypeId
ArfWifiManager::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ArfWifiManager")
    .SetParent<WifiRemoteStationManager> ()
    .SetGroupName ("Wifi")
    .AddConstructor<ArfWifiManager> ()
    .AddAttribute ("TimerThreshold", "The 'timer' threshold in the ARF algorithm.",
                   UintegerValue (15),
                   MakeUintegerAccessor (&ArfWifiManager::m_timerThreshold),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("SuccessThreshold",
                   "The minimum number of sucessfull transmissions to try a new rate.",
                   UintegerValue (10),
                   MakeUintegerAccessor (&ArfWifiManager::m_successThreshold),
                   MakeUintegerChecker<uint32_t> ())
    .AddTraceSource ("Rate",
                     "Traced value for rate changes (b/s)",
                     MakeTraceSourceAccessor (&ArfWifiManager::m_currentRate),
                     "ns3::TracedValueCallback::Uint64")
  ;
  return tid;
}

//Constructor
ArfWifiManager::ArfWifiManager ()
  : WifiRemoteStationManager (),
    m_currentRate (0)
{
  NS_LOG_FUNCTION (this);
}

//Destructor
ArfWifiManager::~ArfWifiManager ()
{
  NS_LOG_FUNCTION (this);
}

/*DoCreateStation is initializing the member variables of class ArfWifiRemoteStation*/
WifiRemoteStation *
ArfWifiManager::DoCreateStation (void) const 
{
  NS_LOG_FUNCTION (this);
  ArfWifiRemoteStation *station = new ArfWifiRemoteStation ();

  station->m_successThreshold = m_successThreshold;
  station->m_timerTimeout = m_timerThreshold;
  station->m_rate = 0;
  station->m_success = 0;
  station->m_failed = 0;
  station->m_recovery = false;
  station->m_retry = 0;
  station->m_timer = 0;

  return station;
}

/*DoReportRtsFailed is called in the event of RTS failure. It isjust an
 informational function which logs the information in case of a RTS 
 failure*/
void
ArfWifiManager::DoReportRtsFailed (WifiRemoteStation *station)
{
  NS_LOG_FUNCTION (this << station);
}
/**
 * It is important to realize that "recovery" mode starts after failure of
 * the first transmission after a rate increase and ends at the first successful
 * transmission. Specifically, recovery mode transcends retransmissions boundaries.
 * Fundamentally, ARF handles each data transmission independently, whether it
 * is the initial transmission of a packet or the retransmission of a packet.
 * The fundamental reason for this is that there is a backoff between each data
 * transmission, be it an initial transmission or a retransmission.
 *
 * \param st the station that we failed to send DATA
 */

/*DoReportDataFailed is called in the even of data transmission sucess.
First it updates transmission statistics variables like m_failed, m_success, etc.
If Recovery mode is enabled (or member variable m_recovery is true) then
it checks if number of retries are greater than 1, then it decrements data rate
to lower available data rate, if it exists. If not in recovery mode, it does 
normal fallback, i.e. only on 2 consecutive data packet failures, it will
decrement data rate to a lower available data rate, if it exists. */
void
ArfWifiManager::DoReportDataFailed (WifiRemoteStation *st)
{
  NS_LOG_FUNCTION (this << st);
  ArfWifiRemoteStation *station = (ArfWifiRemoteStation *)st;
  station->m_timer++;
  station->m_failed++;
  station->m_retry++;
  station->m_success = 0;

  if (station->m_recovery)
    {
      NS_ASSERT (station->m_retry >= 1);
      if (station->m_retry == 1)
        {
          //need recovery fallback
          if (station->m_rate != 0)
            {
              station->m_rate--;
            }
        }
      station->m_timer = 0;
    }
  else
    {
      NS_ASSERT (station->m_retry >= 1);
      if (((station->m_retry - 1) % 2) == 1)
        {
          //need normal fallback
          if (station->m_rate != 0)
            {
              station->m_rate--;
            }
        }
      if (station->m_retry >= 2)
        {
          station->m_timer = 0;
        }
    }
}

/* DoReportRxOk function is called in the event of a successful data packet
reception at the receiving station. This function is also an informational 
function that logs the remote station name, related SNR  and its transmission mode.
*/
void
ArfWifiManager::DoReportRxOk (WifiRemoteStation *station,
                              double rxSnr, WifiMode txMode)
{
  NS_LOG_FUNCTION (this << station << rxSnr << txMode);
}

/* DoReportRtsOk function is called in the event of a successful Rts packet
reception. This function is also an information function that logs the
remote station name, SNR of cts packet, cts transmission mode and SNR of
rts packet.
*/
void ArfWifiManager::DoReportRtsOk (WifiRemoteStation *station,
                                    double ctsSnr, WifiMode ctsMode, double rtsSnr)
{
  NS_LOG_FUNCTION (this << station << ctsSnr << ctsMode << rtsSnr);
  NS_LOG_DEBUG ("station=" << station << " rts ok");
}

/*DoReportDataOk function is  called in the event of a successful ACK packet
reception at sender side. First it updates tansmission statistics like m_failed,
m_success, etc. If number of successful packets tranmitted equals N, or timer
value reaches N, then rate is incremented to a higher available rate, if it exists
and recovery mode is turned on. If switched to a new rate, reset member variables 
m_success and m_timer so that they can contain statistics of new data rate
*/

void ArfWifiManager::DoReportDataOk (WifiRemoteStation *st,
                                     double ackSnr, WifiMode ackMode, double dataSnr)
{
  NS_LOG_FUNCTION (this << st << ackSnr << ackMode << dataSnr);
  ArfWifiRemoteStation *station = (ArfWifiRemoteStation *) st;
  station->m_timer++;
  station->m_success++;
  station->m_failed = 0;
  station->m_recovery = false;
  station->m_retry = 0;
  NS_LOG_DEBUG ("station=" << station << " data ok success=" << station->m_success << ", timer=" << station->m_timer);
  if ((station->m_success == m_successThreshold
       || station->m_timer == m_timerThreshold)
      && (station->m_rate < (station->m_state->m_operationalRateSet.size () - 1)))
    {
      NS_LOG_DEBUG ("station=" << station << " inc rate");
      station->m_rate++;
      station->m_timer = 0;
      station->m_success = 0;
      station->m_recovery = true;
    }
}

/*DoReportFinalRtsFailed function is called in the event when the transmission 
of a RTS has exceeded the maximum number of attempts
*/
void
ArfWifiManager::DoReportFinalRtsFailed (WifiRemoteStation *station)
{
  NS_LOG_FUNCTION (this << station);
}

/*DoReportFinalDataFailed unction is called in the event when the  transmission
 of a data packet has exceeded the maximum number of attempts
*/
void
ArfWifiManager::DoReportFinalDataFailed (WifiRemoteStation *station)
{
  NS_LOG_FUNCTION (this << station);
}

/* This function returns Wifi data transmission vector. Wifi data transmission vector
contains Wifi mode, default transmission power level, Retry count, Preamble 
for sending station, 800, 1, 1, 0, physical channel width, GetAggregation (station), false)
*/
WifiTxVector
ArfWifiManager::DoGetDataTxVector (WifiRemoteStation *st)
{
  NS_LOG_FUNCTION (this << st);
  ArfWifiRemoteStation *station = (ArfWifiRemoteStation *) st;
  uint32_t channelWidth = GetChannelWidth (station);
  if (channelWidth > 20 && channelWidth != 22)
    {
      //avoid to use legacy rate adaptation algorithms for IEEE 802.11n/ac
      channelWidth = 20;
    }
  WifiMode mode = GetSupported (station, station->m_rate);
  if (m_currentRate != mode.GetDataRate (channelWidth))
    {
      NS_LOG_DEBUG ("New datarate: " << mode.GetDataRate (channelWidth));
      m_currentRate = mode.GetDataRate (channelWidth);
    }
  return WifiTxVector (mode, GetDefaultTxPowerLevel (), GetLongRetryCount (station), GetPreambleForTransmission (mode, GetAddress (station)), 800, 1, 1, 0, channelWidth, GetAggregation (station), false);
}

/*This function returns Wifi Rts transmission vector. Wifi Rts transmission vector
contains Wifi mode, default transmission power level, Retry count, Preamble 
for sending station, 800, 1, 1, 0, physical channel width, GetAggregation (station), false)
*/
WifiTxVector
ArfWifiManager::DoGetRtsTxVector (WifiRemoteStation *st)
{
  NS_LOG_FUNCTION (this << st);
  /// \todo we could/should implement the Arf algorithm for
  /// RTS only by picking a single rate within the BasicRateSet.
  ArfWifiRemoteStation *station = (ArfWifiRemoteStation *) st;
  uint32_t channelWidth = GetChannelWidth (station);
  if (channelWidth > 20 && channelWidth != 22)
    {
      //avoid to use legacy rate adaptation algorithms for IEEE 802.11n/ac
      channelWidth = 20;
    }
  WifiTxVector rtsTxVector;
  WifiMode mode;
  if (GetUseNonErpProtection () == false)
    {
      mode = GetSupported (station, 0);
    }
  else
    {
      mode = GetNonErpSupported (station, 0);
    }
  rtsTxVector = WifiTxVector (mode, GetDefaultTxPowerLevel (), GetLongRetryCount (station), GetPreambleForTransmission (mode, GetAddress (station)), 800, 1, 1, 0, channelWidth, GetAggregation (station), false);
  return rtsTxVector;
}

/*IsLowLatency function returns whether this manager is a manager 
designed to work in low-latency environments.
*/
bool
ArfWifiManager::IsLowLatency (void) const
{
  NS_LOG_FUNCTION (this);
  return true;
}

/*This function is called if a wifi standard is being used which uses High Throughput rates.
*/
void
ArfWifiManager::SetHtSupported (bool enable)
{
  //HT is not supported by this algorithm.
  if (enable)
    {
      NS_FATAL_ERROR ("WifiRemoteStationManager selected does not support HT rates");
    }
}

/*This function is called if a wifi standard is being used which uses Very High Throughput rates.
*/
void
ArfWifiManager::SetVhtSupported (bool enable)
{
  //VHT is not supported by this algorithm.
  if (enable)
    {
      NS_FATAL_ERROR ("WifiRemoteStationManager selected does not support VHT rates");
    }
}

/*This function is called if a wifi standard is being used which uses High Efficiency rates.
*/
void
ArfWifiManager::SetHeSupported (bool enable)
{
  //HE is not supported by this algorithm.
  if (enable)
    {
      NS_FATAL_ERROR ("WifiRemoteStationManager selected does not support HE rates");
    }
}

} //namespace ns3
