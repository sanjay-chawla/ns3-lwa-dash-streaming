/*
 * ipv4-ue-list-routing-helper.h
 *
 *  Created on: 4 Jul 2020
 *      Author: sanjay
 */

#ifndef IPV4_UE_LIST_ROUTING_HELPER_H_
#define IPV4_UE_LIST_ROUTING_HELPER_H_

#include "ns3/ipv4-routing-helper.h"
#include <stdint.h>
#include <list>

namespace ns3 {

/**
 * \ingroup ipv4Helpers
 *
 * \brief Helper class that adds ns3::Ipv4ListRouting objects
 *
 * This class is expected to be used in conjunction with
 * ns3::InternetStackHelper::SetRoutingHelper
 */
class Ipv4UeListRoutingHelper : public Ipv4RoutingHelper
{
public:
  /*
   * Construct an Ipv4UeListRoutingHelper used to make installing routing
   * protocols easier.
   */
  Ipv4UeListRoutingHelper ();

  /*
   * Destroy an Ipv4UeListRoutingHelper.
   */
  virtual ~Ipv4UeListRoutingHelper ();

  /**
   * \brief Construct an Ipv4UeListRoutingHelper from another previously
   * initialized instance (Copy Constructor).
   */
  Ipv4UeListRoutingHelper (const Ipv4UeListRoutingHelper &);

  /**
   * \returns pointer to clone of this Ipv4UeListRoutingHelper
   *
   * This method is mainly for internal use by the other helpers;
   * clients are expected to free the dynamic memory allocated by this method
   */
  Ipv4UeListRoutingHelper* Copy (void) const;

  /**
   * \param routing a routing helper
   * \param priority the priority of the associated helper
   *
   * Store in the internal list a reference to the input routing helper
   * and associated priority. These helpers will be used later by
   * the ns3::Ipv4UeListRoutingHelper::Create method to create
   * an ns3::Ipv4ListRouting object and add in it routing protocols
   * created with the helpers.
   */
  void Add (const Ipv4RoutingHelper &routing, int16_t priority);
  /**
   * \param node the node on which the routing protocol will run
   * \returns a newly-created routing protocol
   *
   * This method will be called by ns3::InternetStackHelper::Install
   */
  virtual Ptr<Ipv4RoutingProtocol> Create (Ptr<Node> node) const;
private:
  /**
   * \brief Assignment operator declared private and not implemented to disallow
   * assignment and prevent the compiler from happily inserting its own.
   * \return
   */
  Ipv4UeListRoutingHelper &operator = (const Ipv4UeListRoutingHelper &);

  /**
   * \brief Container for pairs of Ipv4RoutingHelper pointer / priority.
   */
  std::list<std::pair<const Ipv4RoutingHelper *,int16_t> > m_list;
};

} // namespace ns3




#endif /* IPV4_UE_LIST_ROUTING_HELPER_H_ */
