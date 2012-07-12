/**
 * @file
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __avahi_h
#define __avahi_h

#include <vector>

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>

#include <avahi-common/watch.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/thread-watch.h>

#ifdef __WXDEBUG__
//#define PQWX_DEBUG_AVAHI 1
#endif

namespace PQWXAvahi {
  //static bool NssSupport() { return avahi_nss_support(); }

  class Channel {
  public:
    Channel() {}
    virtual ~Channel() {}
    virtual void ServiceFound(const wxString& name, const wxString& type, const wxString& domain, const wxString& hostName, bool local, int interface, int addressFamily, const wxString& addr, wxUint16 port) = 0;
    virtual void ServiceLost(const wxString &name, const wxString& type, const wxString& domain) = 0;
    virtual void ServiceAddressLost(const wxString& name, const wxString& type, const wxString& domain, const wxString& hostName, int interface, int addressFamily) = 0;
    virtual void ServiceRefreshFinished(const wxString& type) = 0;
  };

  class Poller {
  public:
    virtual ~Poller() {}
    virtual const AvahiPoll* GetPollerImpl() const = 0;
  };

  class SimplePoller : public Poller {
  public:
    SimplePoller() { avahiSimplePoller = avahi_simple_poll_new(); }
    ~SimplePoller() { avahi_simple_poll_free(avahiSimplePoller); }

    const AvahiPoll* GetPollerImpl() const { return avahi_simple_poll_get(avahiSimplePoller); }

  private:
    SimplePoller(const SimplePoller&); // Do not copy
    AvahiSimplePoll *avahiSimplePoller;
  };

  class ThreadedPoller : public Poller {
  public:
    ThreadedPoller() { avahiThreadedPoller = avahi_threaded_poll_new(); }
    ~ThreadedPoller() { avahi_threaded_poll_free(avahiThreadedPoller); }

    const AvahiPoll* GetPollerImpl() const { return avahi_threaded_poll_get(avahiThreadedPoller); }
    void Lock() { avahi_threaded_poll_lock(avahiThreadedPoller); }
    void Unlock() { avahi_threaded_poll_unlock(avahiThreadedPoller); }
    void Start() { avahi_threaded_poll_start(avahiThreadedPoller); }
    void Stop() { avahi_threaded_poll_stop(avahiThreadedPoller); }
    void Quit() { avahi_threaded_poll_quit(avahiThreadedPoller); }

  private:
    ThreadedPoller(const ThreadedPoller&); // Do not copy
    AvahiThreadedPoll *avahiThreadedPoller;
  };

  class Client {
  public:
    Client(Channel *channel, const Poller& poller, AvahiClientFlags flags = (AvahiClientFlags)0) : channel(channel)
    {
      int error;
      avahiClient = avahi_client_new(poller.GetPollerImpl(), flags, Callback, (void*) this, &error);
    }
    ~Client() { avahi_client_free(avahiClient); }

    wxString GetHostName() { return wxString(avahi_client_get_host_name(avahiClient), wxConvUTF8); }
    void SetHostName(const wxString& hostname) { avahi_client_set_host_name(avahiClient, hostname.utf8_str()); }

    wxString GetDomainName() { return wxString(avahi_client_get_domain_name(avahiClient), wxConvUTF8); }
    wxString GetHostNameFQDN() { return wxString(avahi_client_get_host_name_fqdn(avahiClient), wxConvUTF8); }
    AvahiClientState GetState() { return avahi_client_get_state(avahiClient); }
    wxString GetVersionString() { return wxString(avahi_client_get_version_string(avahiClient), wxConvUTF8); }

  private:
    Client(const Client&); // Do not copy
    Channel *channel;
    AvahiClient *avahiClient;

    static void Callback(AvahiClient *avahiClient, AvahiClientState state, void *userdata)
    {
#ifdef PQWX_DEBUG_AVAHI
      wxString stateString =
        state == AVAHI_CLIENT_S_REGISTERING ? _T("S_REGISTERING")
        : state == AVAHI_CLIENT_S_RUNNING ? _T("S_RUNNING")
        : state == AVAHI_CLIENT_S_COLLISION ? _T("S_COLLISION")
        : state == AVAHI_CLIENT_FAILURE ? _T("FAILURE")
        : state == AVAHI_CLIENT_CONNECTING ? _T("CONNECTING")
        : _T("?");
        wxLogDebug(_T("AVAHI client callback called (state: %s)"), stateString.c_str());
#endif
    }

    friend class ServiceBrowser;
    friend class ServiceResolver;
  };

  class ServiceResolver {
  public:
    ServiceResolver(Client& client, AvahiIfIndex interface, AvahiProtocol protocol, const char *name, const char *type, const char *domain, AvahiProtocol aprotocol, AvahiLookupFlags flags = (AvahiLookupFlags)0) : client(&client)
    {
      avahiServiceResolver = avahi_service_resolver_new(client.avahiClient, interface, protocol, name, type, domain, aprotocol, flags, Callback, this);
    }
    ~ServiceResolver() { avahi_service_resolver_free(avahiServiceResolver); }
  private:
    ServiceResolver(const ServiceResolver&); // do not copy
    AvahiServiceResolver* avahiServiceResolver;
    Client* client;

    static void Callback(AvahiServiceResolver *r, AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event, const char *name, const char *type, const char *domain, const char *host_name, const AvahiAddress* a, uint16_t port, AvahiStringList *txt, AvahiLookupResultFlags flags, void *userdata)
    {
#ifdef PQWX_DEBUG_AVAHI
      wxString eventString =
        event == AVAHI_RESOLVER_FOUND ? _T("FOUND")
        : event == AVAHI_RESOLVER_FAILURE ? _T("FAILURE")
        : _T("?");
      wxString message = wxString::Format(_T("AVAHI service resolver callback called; interface=%d protocol=%s event=%s"), interface, wxString(avahi_proto_to_string(protocol), wxConvUTF8).c_str(), eventString.c_str());
      if (name != NULL) message << wxString::Format(_T(" name=%s"), wxString(name, wxConvUTF8).c_str());
      if (type != NULL) message << wxString::Format(_T(" type=%s"), wxString(type, wxConvUTF8).c_str());
      if (domain != NULL) message << wxString::Format(_T(" domain=%s"), wxString(domain, wxConvUTF8).c_str());
      if (host_name != NULL) message << wxString::Format(_T(" host_name=%s"), wxString(host_name, wxConvUTF8).c_str());
      wxLogDebug(_T("%s"), message.c_str());
#endif
      if (a != NULL) {
        char addrname[AVAHI_ADDRESS_STR_MAX];
        avahi_address_snprint(addrname, sizeof(addrname), a);
#ifdef PQWX_DEBUG_AVAHI
        const char *protoname = avahi_proto_to_string(a->proto);
        wxLogDebug(_T(" Address: [%s] %s port=%u"), wxString(protoname, wxConvUTF8).c_str(), wxString(addrname, wxConvUTF8).c_str(), port);
#endif

        if (event == AVAHI_RESOLVER_FOUND) {
          ServiceResolver *resolver = static_cast<ServiceResolver*>(userdata);
          resolver->client->channel->ServiceFound(wxString(name, wxConvUTF8), wxString(type, wxConvUTF8), wxString(domain, wxConvUTF8), wxString(host_name, wxConvUTF8), (flags & AVAHI_LOOKUP_RESULT_LOCAL) == AVAHI_LOOKUP_RESULT_LOCAL, interface, avahi_proto_to_af(protocol), wxString(addrname, wxConvUTF8), port);
        }
      }
      else if (event == AVAHI_RESOLVER_FAILURE) {
        ServiceResolver *resolver = static_cast<ServiceResolver*>(userdata);
        resolver->client->channel->ServiceAddressLost(wxString(name, wxConvUTF8), wxString(type, wxConvUTF8), wxString(domain, wxConvUTF8), wxString(host_name, wxConvUTF8), interface, avahi_proto_to_af(protocol));
      }
#ifdef PQWX_DEBUG_AVAHI
      if (txt != NULL) {
        wxLogDebug(_T(" TXT..."));
      }
#endif
    }
  };

  class ServiceBrowser {
  public:
    ServiceBrowser(Client& client, AvahiIfIndex interface, AvahiProtocol protocol, const wxString& type, const wxString& domain, AvahiLookupFlags flags, bool resolveLocals = false) : client(&client), resolveLocals(resolveLocals)
    {
      avahiServiceBrowser = avahi_service_browser_new(client.avahiClient, interface, protocol, type.utf8_str(), domain.utf8_str(), flags, Callback, this);
    }
    ~ServiceBrowser()
    {
      for (std::vector<ServiceResolver*>::iterator iter = resolvers.begin(); iter != resolvers.end(); iter++) {
        delete (*iter);
      }
      avahi_service_browser_free(avahiServiceBrowser);
    }
  private:
    ServiceBrowser(const ServiceBrowser&); // do not copy
    AvahiServiceBrowser* avahiServiceBrowser;
    Client* client;
    bool resolveLocals;
    std::vector<ServiceResolver*> resolvers;

    static void Callback(AvahiServiceBrowser *sb, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *name, const char *type, const char *domain, AvahiLookupResultFlags flags, void *userdata)
    {
#ifdef PQWX_DEBUG_AVAHI
      wxString eventString =
        event == AVAHI_BROWSER_NEW ? _T("NEW")
        : event == AVAHI_BROWSER_REMOVE ? _T("REMOVE")
        : event == AVAHI_BROWSER_CACHE_EXHAUSTED ? _T("CACHE_EXHAUSTED")
        : event == AVAHI_BROWSER_ALL_FOR_NOW ? _T("ALL_FOR_NOW")
        : event == AVAHI_BROWSER_FAILURE ? _T("FAILURE")
        : _T("?");
      wxString flagsString;
      if (flags & AVAHI_LOOKUP_RESULT_CACHED) flagsString << _T("|CACHED");
      if (flags & AVAHI_LOOKUP_RESULT_WIDE_AREA) flagsString << _T("|WIDE_AREA");
      if (flags & AVAHI_LOOKUP_RESULT_MULTICAST) flagsString << _T("|MULTICAST");
      if (flags & AVAHI_LOOKUP_RESULT_LOCAL) flagsString << _T("|LOCAL");
      if (flags & AVAHI_LOOKUP_RESULT_OUR_OWN) flagsString << _T("|OUR_OWN");
      if (flags & AVAHI_LOOKUP_RESULT_STATIC) flagsString << _T("|STATIC");
      if (!flagsString.empty()) flagsString = flagsString.Mid(1);
      wxString message = wxString::Format(_T("AVAHI service browser callback called; interface=%d protocol=%s event=%s flags=%s"), interface, wxString(avahi_proto_to_string(protocol), wxConvUTF8).c_str(), eventString.c_str(), flagsString.c_str());
      if (name != NULL) message << wxString::Format(_T(" name=%s"), wxString(name, wxConvUTF8).c_str());
      if (type != NULL) message << wxString::Format(_T(" type=%s"), wxString(type, wxConvUTF8).c_str());
      if (domain != NULL) message << wxString::Format(_T(" domain=%s"), wxString(domain, wxConvUTF8).c_str());
      wxLogDebug(_T("%s"), message.c_str());
#endif

      if (event == AVAHI_BROWSER_NEW) {
        ServiceBrowser *browser = static_cast<ServiceBrowser*>(userdata);
        if (!(flags & AVAHI_LOOKUP_RESULT_LOCAL) || browser->resolveLocals) {
          browser->resolvers.push_back(new ServiceResolver(*(browser->client), interface, protocol, name, type, domain, protocol));
        }
      }
      else if (event == AVAHI_BROWSER_REMOVE) {
        ServiceBrowser *browser = static_cast<ServiceBrowser*>(userdata);
        browser->client->channel->ServiceLost(wxString(name, wxConvUTF8), wxString(type, wxConvUTF8), wxString(domain, wxConvUTF8));
      }
      else if (event == AVAHI_BROWSER_ALL_FOR_NOW) {
        ServiceBrowser *browser = static_cast<ServiceBrowser*>(userdata);
        browser->client->channel->ServiceRefreshFinished(wxString(type, wxConvUTF8));
      }
    }
  };
};

#endif

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
