#define NOMINMAX

#include <cutils/virtual_net.h>
#include <cutils/wintun.h>

#include <base/strings/sys_string_conversions.h>
#include <base/process/process.h>
#include <base/process/launch.h>

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <vector>

#include <ifdef.h>
#include <iphlpapi.h>
#include <WS2tcpip.h>
#include <dhcpsapi.h>

#pragma comment(lib, "iphlpapi.lib")

/* 协议常量 */
#define IP_PROTO_UDP        17      /* UDP 协议号 */
#define UDP_DHCP_CLIENT     68      /* DHCP 客户端端口 */
#define UDP_DHCP_SERVER     67      /* DHCP 服务器端口 */
#define DHCP_CHADDR_OFFSET  28      /* DHCP 报文中 MAC 地址偏移 */
#define MAC_LEN             6       /* 标准MAC长度 */

#define ETH_P_IP 0x0800
#define ETH_P_IPV6 0x86DD
#define ETH_TYPE_ARP  0x0806   // 以太网类型：ARP
#define ARP_HW_ETH    0x0001   // 硬件类型：以太网
#define ARP_PROTO_IP4 0x0800   // 协议类型：IPv4
#define ARP_OP_REQUEST 1       // ARP 请求
#define ARP_OP_REPLY   2       // ARP 应答

#pragma pack(push, 1)
/* IPv4 头部（标准20字节，无选项）*/
struct iphdr
{
    uint8_t  ihl : 4;
    uint8_t  version : 4;
    uint8_t  tos;
    uint16_t tot_len;
    uint16_t id;
    uint16_t frag_off;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t check;
    uint8_t  saddr[4];
    uint8_t  daddr[4];
};

/* UDP 头部 */
struct udphdr
{
    uint16_t source;
    uint16_t dest;
    uint16_t len;
    uint16_t check;
};

struct arp_message
{
    uint16_t hw_type;       /* 0x0001 以太网   */
    uint16_t proto_type;    /* 0x0800 IPv4    */
    uint8_t  hw_len;        /* 6 (MAC 地址长度) */
    uint8_t  proto_len;     /* 4 (IP 地址长度)  */
    uint16_t op;            /* 1=请求, 2=应答   */
    uint8_t  sender_mac[6]; /* 发送端 MAC       */
    uint32_t sender_ip;     /* 发送端 IP        */
    uint8_t  target_mac[6]; /* 目标 MAC        */
    uint32_t target_ip;     /* 目标 IP         */
};
#pragma pack(pop)

static void CALLBACK ConsoleLogger
    (
    _In_ WINTUN_LOGGER_LEVEL Level,
    _In_ DWORD64 Timestamp,
    _In_z_ const WCHAR* LogLine
    )
{
    switch (Level)
    {
    case WINTUN_LOG_INFO:
        LOG(INFO) << LogLine;
        break;
    case WINTUN_LOG_WARN:
        LOG(WARNING) << LogLine;
        break;
    case WINTUN_LOG_ERR:
        LOG(ERROR) << LogLine;
        break;
    default:
        LOG(INFO) << LogLine;
        return;
    }
}

struct WintunContext
{
    int id = 0;
    WINTUN_ADAPTER_HANDLE adapter = nullptr;
    WINTUN_SESSION_HANDLE session = nullptr;
    HANDLE read_event = nullptr;
    std::vector<uint8_t> m_mac_addr;
    std::wstring name;
};

class virtual_net_manager
{

public:

    static virtual_net_manager& get_instance();

    virtual_net_manager();

    int open_virtual_net
        (
        const char* a_name
        );

    int start_virtual_net
        (
        int a_virtual_net
        );

    int retrieve_virtual_net_mac
        (
        int a_virtual_net_id,
        uint8_t* a_mac_original,
        uint16_t a_mac_original_size
        );

    int send_ip_packet_virtual_net
        (
        int a_virtual_net,
        const char* a_buf,
        uint16_t a_len
        );

    int handle_arp_packet_virtual_net
        (
        int                  a_virtual_net_id,
        std::vector<uint8_t> a_mac_addr,
        std::vector<uint8_t> a_arp_packet
        );

    void set_ip_packet_callback( ip_packet_callback a_callback )
    {
        std::lock_guard locker( m_mutex );
        m_ip_packet_callback = a_callback;
    }

    int receive_ip_virtual_net
        (
        int          a_virtual_net,
        const char** a_buf,
        uint16_t*    a_len
        );

    void release_packet( int a_virtual_net_id, uint16_t a_packet_type, const char* a_packet );

    void close_virtual_net
        (
        int a_virtual_net
        );

    void post_task( std::function<void()> a_task );

private:

    void load_symbols();

    void clear();

    void start_read_thread();

    void stop_read_thread();

    void read_ip_packet_loop();

    void try_to_retrieve_mac_addr( int a_virtual_net_id );

private:

    WINTUN_CREATE_ADAPTER_FUNC*             m_WintunCreateAdapter = nullptr;
    WINTUN_CLOSE_ADAPTER_FUNC*              m_WintunCloseAdapter = nullptr;
    WINTUN_OPEN_ADAPTER_FUNC*               m_WintunOpenAdapter = nullptr;
    WINTUN_GET_ADAPTER_LUID_FUNC*           m_WintunGetAdapterLUID = nullptr;
    WINTUN_GET_RUNNING_DRIVER_VERSION_FUNC* m_WintunGetRunningDriverVersion = nullptr;
    WINTUN_DELETE_DRIVER_FUNC*              m_WintunDeleteDriver = nullptr;
    WINTUN_SET_LOGGER_FUNC*                 m_WintunSetLogger = nullptr;
    WINTUN_START_SESSION_FUNC*              m_WintunStartSession = nullptr;
    WINTUN_END_SESSION_FUNC*                m_WintunEndSession = nullptr;
    WINTUN_GET_READ_WAIT_EVENT_FUNC*        m_WintunGetReadWaitEvent = nullptr;
    WINTUN_RECEIVE_PACKET_FUNC*             m_WintunReceivePacket = nullptr;
    WINTUN_RELEASE_RECEIVE_PACKET_FUNC*     m_WintunReleaseReceivePacket = nullptr;
    WINTUN_ALLOCATE_SEND_PACKET_FUNC*       m_WintunAllocateSendPacket = nullptr;
    WINTUN_SEND_PACKET_FUNC*                m_WintunSendPacket = nullptr;

    HMODULE m_Wintun = NULL;
    HANDLE m_signaling_event;
    bool m_running = false;

    std::shared_mutex m_mutex;
    int m_next_id = 0;
    ip_packet_callback m_ip_packet_callback = nullptr;
    std::vector<WintunContext> m_created_adapters;
    std::vector<std::function<void()>> m_posted_tasks;
    std::thread m_read_thread;
};

virtual_net_manager& virtual_net_manager::get_instance()
{
    static virtual_net_manager instance;
    return instance;
}

void virtual_net_manager::load_symbols()
{
    if (m_Wintun != NULL)
    {
        return;
    }

    m_Wintun =
        LoadLibraryExW( L"wintun.dll", NULL, LOAD_LIBRARY_SEARCH_APPLICATION_DIR |
            LOAD_LIBRARY_SEARCH_SYSTEM32 |
            LOAD_LIBRARY_SEARCH_USER_DIRS );
    if (m_Wintun == NULL)
    {
        return;
    }

    WINTUN_CREATE_ADAPTER_FUNC* WintunCreateAdapter = nullptr;
    WINTUN_CLOSE_ADAPTER_FUNC* WintunCloseAdapter = nullptr;
    WINTUN_OPEN_ADAPTER_FUNC* WintunOpenAdapter = nullptr;
    WINTUN_GET_ADAPTER_LUID_FUNC* WintunGetAdapterLUID = nullptr;
    WINTUN_GET_RUNNING_DRIVER_VERSION_FUNC* WintunGetRunningDriverVersion = nullptr;
    WINTUN_DELETE_DRIVER_FUNC* WintunDeleteDriver = nullptr;
    WINTUN_SET_LOGGER_FUNC* WintunSetLogger = nullptr;
    WINTUN_START_SESSION_FUNC* WintunStartSession = nullptr;
    WINTUN_END_SESSION_FUNC* WintunEndSession = nullptr;
    WINTUN_GET_READ_WAIT_EVENT_FUNC* WintunGetReadWaitEvent = nullptr;
    WINTUN_RECEIVE_PACKET_FUNC* WintunReceivePacket = nullptr;
    WINTUN_RELEASE_RECEIVE_PACKET_FUNC* WintunReleaseReceivePacket = nullptr;
    WINTUN_ALLOCATE_SEND_PACKET_FUNC* WintunAllocateSendPacket = nullptr;
    WINTUN_SEND_PACKET_FUNC* WintunSendPacket = nullptr;

#define X(Name) ((*(FARPROC *)&Name = GetProcAddress(m_Wintun, #Name)) == NULL)
    if (X(WintunCreateAdapter) || X(WintunCloseAdapter) || X(WintunOpenAdapter) || X(WintunGetAdapterLUID) ||
        X(WintunGetRunningDriverVersion) || X(WintunDeleteDriver) || X(WintunSetLogger) || X(WintunStartSession) ||
        X(WintunEndSession) || X(WintunGetReadWaitEvent) || X(WintunReceivePacket) || X(WintunReleaseReceivePacket) ||
        X(WintunAllocateSendPacket) || X(WintunSendPacket))
#undef X
    {
        DWORD LastError = GetLastError();
        FreeLibrary(m_Wintun);
        SetLastError( LastError );
        m_Wintun = NULL;
        clear();
        return;
    }

    m_WintunCreateAdapter = WintunCreateAdapter;
    m_WintunCloseAdapter = WintunCloseAdapter;
    m_WintunOpenAdapter = WintunOpenAdapter;
    m_WintunGetAdapterLUID = WintunGetAdapterLUID;
    m_WintunGetRunningDriverVersion = WintunGetRunningDriverVersion;
    m_WintunDeleteDriver = WintunDeleteDriver;
    m_WintunSetLogger = WintunSetLogger;
    m_WintunStartSession = WintunStartSession;
    m_WintunEndSession = WintunEndSession;
    m_WintunGetReadWaitEvent = WintunGetReadWaitEvent;
    m_WintunReceivePacket = WintunReceivePacket;
    m_WintunReleaseReceivePacket = WintunReleaseReceivePacket;
    m_WintunAllocateSendPacket = WintunAllocateSendPacket;
    m_WintunSendPacket = WintunSendPacket;

    m_WintunSetLogger(ConsoleLogger);
    return;
}

virtual_net_manager::virtual_net_manager()
{
    m_signaling_event = CreateEventW( NULL, TRUE, FALSE, NULL );
    load_symbols();
}

int virtual_net_manager::open_virtual_net(const char* a_name )
{
    start_read_thread();
    auto name = base::SysNativeMBToWide(a_name);
    WINTUN_ADAPTER_HANDLE Adapter = m_WintunCreateAdapter(name.c_str(), L"Generic TUN", nullptr);
    if (!Adapter)
    {
        LOG(ERROR) << ("Failed to create adapter. error code: ") << GetLastError();
        return 0;
    }

    WintunContext context;
    context.adapter = Adapter;
    context.name = name;

    std::lock_guard locker(m_mutex);
    context.id = ++m_next_id;
    m_created_adapters.push_back( context );

    SetEvent( m_signaling_event );
    return context.id;
}

int virtual_net_manager::start_virtual_net
    (
    int a_virtual_net
    )
{
    bool found = false;
    bool mac_retrieved = false;
    WINTUN_ADAPTER_HANDLE adapter = NULL;
    NET_LUID InterfaceLuid;
    BYTE macOut[MAC_LEN] = { 0 };
    std::unique_lock locker( m_mutex );
    for( auto it = m_created_adapters.begin(); it != m_created_adapters.end(); ++it )
    {
        if( it->id == a_virtual_net )
        {
            adapter = it->adapter;
            found = true;
            break;
        }
    }
    locker.unlock();
    if( !found )
    {
        return 0;
    }

    WINTUN_SESSION_HANDLE session = m_WintunStartSession( adapter, 0x400000 );
    m_WintunGetAdapterLUID( adapter, &InterfaceLuid );
    MIB_IF_ROW2 ifRow;
    ZeroMemory( &ifRow, sizeof( ifRow ) );
    ifRow.InterfaceLuid = InterfaceLuid;
    auto status = GetIfEntry2( &ifRow );
    if( status == NO_ERROR )
    {
        if( ifRow.PhysicalAddressLength > 6 )
        {
            mac_retrieved = true;
            CopyMemory( macOut, ifRow.PhysicalAddress, MAC_LEN );
        }
    }

    if( !session )
    {
        m_WintunCloseAdapter( ( WINTUN_ADAPTER_HANDLE )a_virtual_net );
        std::lock_guard locker( m_mutex );
        for( auto it = m_created_adapters.begin(); it != m_created_adapters.end(); ++it )
        {
            if( it->id == a_virtual_net )
            {
                m_created_adapters.erase( it );
                break;
            }
        }
        a_virtual_net = 0;
        return -1;
    }

    locker.lock();
    for( auto it = m_created_adapters.begin(); it != m_created_adapters.end(); ++it )
    {
        if( it->id == a_virtual_net )
        {
            it->session = session;
            it->read_event = m_WintunGetReadWaitEvent( session );
            if( mac_retrieved )
            {
                it->m_mac_addr.resize( MAC_LEN );
                CopyMemory( it->m_mac_addr.data(), macOut, MAC_LEN );
            }
            else
            {
                it->m_mac_addr.clear();
            }
            break;
        }
    }
    locker.unlock();

    /* 注意：需要管理员权限 */
    int code = 0x00;
    bool use_dhcp = true;
    if( use_dhcp )
    {
        //base::LaunchOptions opts;
        //base::LaunchProcess( L"C:\\Windows\\System32\\netsh interface ip set address \"bt-pan\" dhcp", opts );
        //auto process = base::LaunchProcess( L"C:\\Windows\\System32\\ipconfig /renew \"bt-pan\"", opts );
        //process.WaitForExitWithTimeout( base::TimeDelta::FromSeconds( 3 ), &code );
        system( "C:\\Windows\\System32\\netsh interface ip set address \"bt-pan\" dhcp" );
        system( "C:\\Windows\\System32\\ipconfig /renew \"bt-pan\"" );
    }
    else
    {
        MIB_UNICASTIPADDRESS_ROW AddressRow;
        InitializeUnicastIpAddressEntry( &AddressRow );
        m_WintunGetAdapterLUID( adapter, &AddressRow.InterfaceLuid );
        AddressRow.Address.Ipv4.sin_family = AF_INET;
        const wchar_t* ip_wstr = L"10.6.7.7"; /*use this value, but you can change it.*/
        InetPton( AF_INET, ip_wstr, &AddressRow.Address.Ipv4.sin_addr );
        AddressRow.OnLinkPrefixLength = 24;
        AddressRow.DadState = IpDadStatePreferred;
        DWORD ret = CreateUnicastIpAddressEntry( &AddressRow );
        if( ret != ERROR_SUCCESS && ret != ERROR_OBJECT_ALREADY_EXISTS )
        {
            LOG( ERROR ) << "cannot set IP address.";
        }
    }

    SetEvent( m_signaling_event );
    return 0;
}

int virtual_net_manager::retrieve_virtual_net_mac
    (
    int a_virtual_net_id,
    uint8_t* a_mac_original,
    uint16_t a_mac_original_size
    )
{
    if( a_mac_original_size != MAC_LEN )
    {
        return -1;
    }

    bool retrieved = false;

    std::shared_lock locker( m_mutex );
    for( auto it = m_created_adapters.begin(); it != m_created_adapters.end(); ++it )
    {
        if( it->id == a_virtual_net_id )
        {
            if( it->m_mac_addr.size() >= MAC_LEN )
            {
                memcpy( a_mac_original, it->m_mac_addr.data(), a_mac_original_size );
                retrieved = true;
            }
            break;
        }
    }
    locker.unlock();

    if( !retrieved )
    {
        try_to_retrieve_mac_addr( a_virtual_net_id );
    }

    locker.lock();
    for( auto it = m_created_adapters.begin(); it != m_created_adapters.end(); ++it )
    {
        if( it->id == a_virtual_net_id )
        {
            if( it->m_mac_addr.size() >= MAC_LEN )
            {
                memcpy( a_mac_original, it->m_mac_addr.data(), a_mac_original_size );
                retrieved = true;
            }
            break;
        }
    }
    locker.unlock();

    return retrieved ? 0 : -1;
}

int virtual_net_manager::send_ip_packet_virtual_net
    (
    int a_virtual_net,
    const char* a_buf,
    uint16_t a_len
    )
{
    const char* ip_packet = a_buf;
    uint16_t ip_len = a_len;

    std::shared_lock locker( m_mutex );
    for( auto it = m_created_adapters.begin(); it != m_created_adapters.end(); ++it )
    {
        if( it->id == a_virtual_net )
        {
            BYTE* send_buf = m_WintunAllocateSendPacket( it->session, ip_len );
            if( !send_buf )
            {
                /* 缓冲区满，返回拥塞 */
                return -2;  // FORWARD_CONGEST
            }

            /*复制 IP 数据包*/
            memcpy( send_buf, ip_packet, ip_len );

            /*发送到 Wintun（进入 Windows 网络栈）*/
            m_WintunSendPacket( it->session, send_buf );
            return a_len;
        }
    }

    return -1;
}

int virtual_net_manager::handle_arp_packet_virtual_net
    (
    int                  a_virtual_net_id,
    std::vector<uint8_t> a_mac_addr,
    std::vector<uint8_t> a_arp_packet
    )
{
    if( a_arp_packet.size() != sizeof( arp_message ) )
    {
        LOG( ERROR ) << "not an arp message!";
        return -1;
    }

    if( a_mac_addr.size() < 6 )
    {
        LOG( ERROR ) << "wrong MAC address!";
        return -1;
    }

    arp_message* received_arp = ( arp_message* )a_arp_packet.data();
    if( ntohs( received_arp->hw_type ) != ARP_HW_ETH )
    {
        LOG( ERROR ) << "not an arp message";
        return -1;
    }

    if( ntohs( received_arp->proto_type ) != ARP_PROTO_IP4 )
    {
        LOG( ERROR ) << "not an arp message";
        return -1;
    }

    if( ntohs( received_arp->op ) != ARP_OP_REQUEST )
    {
        LOG( ERROR ) << "we only handle request packet now";
        return -1;
    }

    arp_message response = *received_arp;

    response.target_ip = received_arp->sender_ip;
    memcpy( response.target_mac, received_arp->sender_mac, 6 );

    response.sender_ip = received_arp->target_ip;
    memcpy( response.sender_mac, a_mac_addr.data(), 6 );

    response.op = ntohs( ARP_OP_REPLY );

    char* buffer = new char[sizeof( arp_message )];
    memcpy( buffer, &response, sizeof( arp_message ) );
    m_ip_packet_callback( a_virtual_net_id, ETH_TYPE_ARP, buffer, sizeof( arp_message ) );
    return 0;
}

int virtual_net_manager::receive_ip_virtual_net
    (
    int          a_virtual_net,
    const char** a_buf,
    uint16_t*    a_len
    )
{
    *a_buf = nullptr;
    *a_len = 0;
    WINTUN_SESSION_HANDLE Session = NULL;
    HANDLE read_event = NULL;
    std::shared_lock locker( m_mutex );
    for( auto it = m_created_adapters.begin(); it != m_created_adapters.end(); ++it )
    {
        if( it->id == a_virtual_net )
        {
            Session = it->session;
            read_event = it->read_event;
            break;
        }
    }
    locker.unlock();

    if( Session == NULL )
    {
        return -1;
    }

    HANDLE WaitHandles[] = { read_event };
    DWORD PacketSize;
    BYTE* Packet = 0;

    do
    {
        Packet = m_WintunReceivePacket( Session, &PacketSize );
        if( Packet )
        {
            *a_buf = ( const char* )Packet;
            *a_len = PacketSize;
            return PacketSize;
        }
        else
        {
            DWORD LastError = GetLastError();
            switch( LastError )
            {
            case ERROR_NO_MORE_ITEMS:
                if( WaitForMultipleObjects( _countof( WaitHandles ), WaitHandles, FALSE, INFINITE ) == WAIT_OBJECT_0 )
                {
                    Packet = m_WintunReceivePacket( Session, &PacketSize );
                    if( Packet )
                    {
                        *a_buf = ( const char* )Packet;
                        *a_len = PacketSize;
                        return PacketSize;
                    }
                }
            break;
            default:
                LOG( ERROR ) << "Packet read failed. error: " << LastError;
                return LastError;
            }
        }
    } while( true );

    return -1;
}

void virtual_net_manager::release_packet( int a_virtual_net_id, uint16_t a_packet_type, const char* a_packet )
{
    if( a_packet_type == ETH_TYPE_ARP )
    {
        delete a_packet;
        return;
    }

    std::shared_lock locker( m_mutex );
    for( auto it = m_created_adapters.begin(); it != m_created_adapters.end(); ++it )
    {
        if( it->id == a_virtual_net_id )
        {
            auto Session = it->session;
            m_WintunReleaseReceivePacket( Session, ( const BYTE* )a_packet );
            break;
        }
    }
    locker.unlock();
}

void virtual_net_manager::close_virtual_net
    (
    int a_virtual_net
    )
{
    std::unique_lock locker( m_mutex );
    for( auto it = m_created_adapters.begin(); it != m_created_adapters.end(); ++it )
    {
        if( it->id == a_virtual_net )
        {
            m_WintunEndSession( it->session );
            m_WintunCloseAdapter( it->adapter );
            m_created_adapters.erase( it );
            break;
        }
    }
}

void virtual_net_manager::post_task( std::function<void()> a_task )
{
    std::unique_lock locker( m_mutex );
    m_posted_tasks.push_back( std::move( a_task ) );
    locker.unlock();

    SetEvent( m_signaling_event );
}

void virtual_net_manager::clear()
{
    m_WintunCreateAdapter = nullptr;
    m_WintunCloseAdapter = nullptr;
    m_WintunOpenAdapter = nullptr;
    m_WintunGetAdapterLUID = nullptr;
    m_WintunGetRunningDriverVersion = nullptr;
    m_WintunDeleteDriver = nullptr;
    m_WintunSetLogger = nullptr;
    m_WintunStartSession = nullptr;
    m_WintunEndSession = nullptr;
    m_WintunGetReadWaitEvent = nullptr;
    m_WintunReceivePacket = nullptr;
    m_WintunReleaseReceivePacket = nullptr;
    m_WintunAllocateSendPacket = nullptr;
    m_WintunSendPacket = nullptr;
}

void virtual_net_manager::start_read_thread()
{
    m_running = true;
    if( m_read_thread.joinable() )
    {
        LOG( INFO ) << "already running";
        return;
    }

    m_read_thread = std::thread( &virtual_net_manager::read_ip_packet_loop,
        std::ref( virtual_net_manager::get_instance() ) );
}

void virtual_net_manager::stop_read_thread()
{
    m_running = false;
    SetEvent( m_signaling_event );
    if( m_read_thread.joinable() )
    {
        m_read_thread.join();
    }
}

void virtual_net_manager::read_ip_packet_loop()
{
    std::vector<HANDLE> waiting_handles;
    BYTE* Packet = nullptr;
    DWORD PacketSize = 0;
    int virtual_net_id = 0;
    std::vector<std::function<void()>> posted_tasks;

    while( m_running )
    {
        posted_tasks.clear();
        Packet = nullptr;
        PacketSize = 0;
        std::unique_lock locker( m_mutex, std::defer_lock );
        waiting_handles.clear();

        locker.lock();
        waiting_handles.push_back( m_signaling_event );
        std::swap( posted_tasks, m_posted_tasks );
        for( auto it = m_created_adapters.begin(); it != m_created_adapters.end(); ++it )
        {
            if( it->session == NULL )
            {
                continue;
            }

            Packet = m_WintunReceivePacket( it->session, &PacketSize );
            if( Packet )
            {
                virtual_net_id = it->id;
                break;
            }
            else
            {
                waiting_handles.push_back( it->read_event );
            }
        }
        locker.unlock();

        for( auto&& task : posted_tasks )
        {
            task();
        }

        if( Packet )
        {
            uint8_t version = ( Packet[0] >> 4 ) & 0x0F;
            uint16_t proto_type = 0x00;
            if( version == 4 )
            {
                proto_type = ETH_P_IP;
            }
            else if( version == 6 )
            {
                proto_type = ETH_P_IPV6;
            }
            m_ip_packet_callback( virtual_net_id, proto_type, ( const char* )Packet, PacketSize );
            continue;
        }

        if( WaitForMultipleObjects( waiting_handles.size(), waiting_handles.data(), FALSE, 5000 )
            == WAIT_OBJECT_0 )
        {
            continue;
        }
    }
}

void virtual_net_manager::try_to_retrieve_mac_addr( int a_virtual_net_id )
{
    bool found = false;
    bool mac_retrieved = false;
    WINTUN_ADAPTER_HANDLE adapter = nullptr;
    NET_LUID InterfaceLuid;
    BYTE macOut[MAC_LEN] = { 0 };
    std::unique_lock locker( m_mutex );
    for( auto it = m_created_adapters.begin(); it != m_created_adapters.end(); ++it )
    {
        if( it->id == a_virtual_net_id )
        {
            adapter = it->adapter;
            found = true;
            m_WintunGetAdapterLUID( adapter, &InterfaceLuid );
            MIB_IF_ROW2 ifRow;
            ZeroMemory( &ifRow, sizeof( ifRow ) );
            ifRow.InterfaceLuid = InterfaceLuid;
            auto status = GetIfEntry2( &ifRow );
            if( status == NO_ERROR )
            {
                if( ifRow.PhysicalAddressLength >= MAC_LEN )
                {
                    mac_retrieved = true;
                    CopyMemory( macOut, ifRow.PhysicalAddress, MAC_LEN );
                }
            }

            if( mac_retrieved )
            {
                it->m_mac_addr.resize( MAC_LEN );
                memcpy( it->m_mac_addr.data(), macOut, MAC_LEN );
            }
            break;
        }
    }
    locker.unlock();

    if( !found )
    {
        LOG( ERROR ) << "no such net id: " << a_virtual_net_id;
    }
}

#ifdef __cplusplus
extern "C" {
#endif

    int open_virtual_net(const char* a_name )
    {
        return virtual_net_manager::get_instance().open_virtual_net( a_name );
    }

    int start_virtual_net
        (
        int a_virtual_net
        )
    {
        return virtual_net_manager::get_instance().start_virtual_net( a_virtual_net );
    }

    int retrieve_virtual_net_mac
        (
        int a_virtual_net_id,
        uint8_t* a_mac_original,
        uint16_t a_mac_original_size
        )
    {
        return virtual_net_manager::get_instance().retrieve_virtual_net_mac(
            a_virtual_net_id, a_mac_original, a_mac_original_size );
    }

    int send_ip_packet_virtual_net
        (
        int a_virtual_net,
        const char* a_buf,
        uint16_t a_len
        )
    {
        return virtual_net_manager::get_instance().send_ip_packet_virtual_net( a_virtual_net, a_buf, a_len );
    }

    int send_arp_packet_virtual_net
        (
        int         a_virtual_net_id,
        const char* a_local_mac,
        uint16_t    a_local_mac_size,
        const char* a_buffer,
        uint16_t    a_len
        )
    {
        std::vector<uint8_t> buffer;
        std::vector<uint8_t> mac_buffer;
        buffer.resize( a_len );
        mac_buffer.resize( a_local_mac_size );
        memcpy( buffer.data(), a_buffer, a_len );
        memcpy( mac_buffer.data(), a_local_mac, a_local_mac_size );
        std::function<void()> task = std::bind(&virtual_net_manager::handle_arp_packet_virtual_net,
            std::ref( virtual_net_manager::get_instance() ), a_virtual_net_id,
            std::move( mac_buffer ), std::move( buffer) );
        virtual_net_manager::get_instance().post_task( task );
        return 0;
    }

    void set_ip_packet_callback( ip_packet_callback a_callback )
    {
        return virtual_net_manager::get_instance().set_ip_packet_callback( a_callback );
    }

    int receive_ip_virtual_net
        (
        int a_virtual_net,
        const char** a_buf,
        uint16_t* a_len
        )
    {
        return virtual_net_manager::get_instance().receive_ip_virtual_net( a_virtual_net, a_buf, a_len );
    }

    void release_packet( int a_virtual_net_id, uint16_t a_packet_type, const char* a_packet )
    {
        return virtual_net_manager::get_instance().release_packet( a_virtual_net_id, a_packet_type, a_packet );
    }

    void close_virtual_net
        (
        int a_virtual_net
        )
    {
        return virtual_net_manager::get_instance().close_virtual_net( a_virtual_net );
    }

    uint16_t simple_udp_checksum( const uint8_t* ip_header, const uint8_t* udp_packet, uint16_t udp_len )
    {
        uint32_t sum = 0;

        // 伪头部 12 字节 (IPv4)
        // saddr (4字节) + daddr (4字节) + 0 + protocol + udp_len
        sum += ( ip_header[12] << 8 ) | ip_header[13];
        sum += ( ip_header[14] << 8 ) | ip_header[15];
        sum += ( ip_header[16] << 8 ) | ip_header[17];
        sum += ( ip_header[18] << 8 ) | ip_header[19];
        sum += 0x0011;                      // protocol = UDP = 17
        sum += udp_len;                     // 注意：这里是主机序，不是网络序

        // UDP header + 数据 (把 check 字段视为 0)
        const uint8_t* p = udp_packet;
        int remaining = udp_len;

        // 先处理前 6 字节 (source + dest + len)
        sum += ( p[0] << 8 ) | p[1];   // source port
        sum += ( p[2] << 8 ) | p[3];   // dest port
        sum += ( p[4] << 8 ) | p[5];   // length
        // p[6] 和 p[7] 是 check → 我们跳过，相当于清零

        p += 8;
        remaining -= 8;

        // 剩余 payload，按 16bit 累加
        while( remaining >= 2 ) {
            sum += ( p[0] << 8 ) | p[1];
            p += 2;
            remaining -= 2;
        }

        // 最后一个奇数字节（如果有）
        if( remaining == 1 ) {
            sum += ( p[0] << 8 );
        }

        // 折叠进位
        while( sum >> 16 ) {
            sum = ( sum & 0xFFFF ) + ( sum >> 16 );
        }

        uint16_t result = ~( uint16_t )sum;

        // RFC 768：全0 转为 0xFFFF
        return ( result == 0 ) ? 0xFFFF : result;
    }

    // ====================================================================================
    // 简单版 IP header checksum 计算（适配 raw buffer）
    // ====================================================================================
    uint16_t simple_ip_checksum( const uint8_t* a_ip_packet, uint16_t a_ip_packet_size )
    {
        // IP header 长度必须至少 20 字节，且是 4 的倍数
        if( a_ip_packet_size < 20 || ( a_ip_packet_size % 4 ) != 0 ) {
            return 0;  // 非法输入，返回 0（或你可以根据需要抛异常/返回特定值）
        }

        // 从第一个字节开始读取 version + IHL
        uint8_t version_ihl = a_ip_packet[0];
        uint8_t ihl = version_ihl & 0x0F;          // 低 4 位：头部长度（单位：4字节）
        uint16_t header_len = ihl * 4;

        // 防止输入的包比头部还短（虽然前面已经检查 >=20）
        if( header_len > a_ip_packet_size ) {
            return 0;
        }

        uint32_t sum = 0;
        const uint8_t* p = a_ip_packet;

        // 按 16-bit 累加整个头部（checksum 字段位置 10-11 字节会自然被当作 0 处理）
        for( uint16_t i = 0; i < header_len; i += 2 ) {
            uint16_t word = ( p[i] << 8 ) | p[i + 1];

            // 如果是 checksum 字段位置（偏移 10），强制视为 0
            // （虽然很多实现不强制，因为输入包通常已包含正确值，但为了计算“应该的值”最好清零）
            if( i == 10 ) {
                word = 0;
            }

            sum += word;
        }

        // 进位折叠
        while( sum >> 16 ) {
            sum = ( sum & 0xFFFF ) + ( sum >> 16 );
        }

        uint16_t result = ~( uint16_t )sum;

        // IP header checksum 规范：全0 应为 0xFFFF（但实际很少见）
        return ( result == 0 ) ? 0xFFFF : result;
    }

    int replace_mac_for_ip_packet
        (
        uint8_t* a_ip_packet,
        uint16_t a_ip_packet_size,
        uint8_t* a_mac_to_replace,
        uint16_t a_mac_size,
        uint8_t* a_mac_original,
        uint16_t a_mac_original_size
        )
    {
        /* 最小长度判断：IP头(20) + UDP头(8) + DHCP固定头(236) */
        constexpr uint16_t MIN_TOTAL_LEN = ( 20 + 8 + 236 );

        /* ===================== 第一步：基础合法性校验 ===================== */
        /* 空指针检查 */
        if( a_ip_packet == NULL || a_mac_to_replace == NULL ) {
            return -1;
        }

        /* 长度过短，不是合法的IP+UDP+DHCP包 */
        if( a_ip_packet_size < MIN_TOTAL_LEN ) {
            return -1;
        }

        /* MAC 必须是6字节 */
        if( a_mac_size != MAC_LEN || a_mac_original_size != MAC_LEN ) {
            return -1;
        }

        /* ===================== 第二步：解析IP头 ===================== */
        struct iphdr* ip_hdr = ( struct iphdr* )a_ip_packet;

        /* 版本必须是 IPv4 */
        if( ip_hdr->version != 4 ) {
            return -1;
        }

        /* IP头长度必须合法（至少20字节）*/
        uint8_t ip_header_len = ip_hdr->ihl * 4;
        if( ip_header_len < 20 ) {
            return -1;
        }

        /* 确保IP包长度不越界 */
        if( ( uint16_t )( ip_header_len + 8 + 236 ) > a_ip_packet_size ) {
            return -1;
        }

        /* ===================== 第三步：判断是否为UDP ===================== */
        if( ip_hdr->protocol != IP_PROTO_UDP ) {
            return -1;
        }

        /* ===================== 第四步：解析UDP头 ===================== */
        struct udphdr* udp_hdr = ( struct udphdr* )( a_ip_packet + ip_header_len );

        /* 判断是否是 DHCP 端口（67/68）*/
        uint16_t src_port = ntohs( udp_hdr->source );
        uint16_t dst_port = ntohs( udp_hdr->dest );
        if( !( ( src_port == UDP_DHCP_CLIENT && dst_port == UDP_DHCP_SERVER ) ||
            ( src_port == UDP_DHCP_SERVER && dst_port == UDP_DHCP_CLIENT ) ) )
        {
            return -1;
        }

        /* ===================== 第五步：定位 DHCP 报文并替换 MAC ===================== */
        uint8_t* dhcp_msg = ( uint8_t* )udp_hdr + sizeof( struct udphdr );
        uint8_t* chaddr = dhcp_msg + DHCP_CHADDR_OFFSET;

        if( a_mac_to_replace[0] == 0x00 &&
            a_mac_to_replace[1] == 0x00 &&
            a_mac_to_replace[2] == 0x00 &&
            a_mac_to_replace[3] == 0x00 &&
            a_mac_to_replace[4] == 0x00 &&
            a_mac_to_replace[5] == 0x00
            )
        {
            dhcp_msg[2] = 0x00;
        }
        else
        {
            dhcp_msg[2] = MAC_LEN;
        }

        /* 替换 DHCP 内部的 MAC 地址（CHADDR）*/
        memcpy( a_mac_original, chaddr, MAC_LEN );
        memcpy( chaddr, a_mac_to_replace, MAC_LEN );

        uint16_t udp_checksum = simple_udp_checksum( a_ip_packet, a_ip_packet + ip_header_len, ntohs( udp_hdr->len ) );
        udp_hdr->check = htons( udp_checksum );

        uint16_t ip_checksum = simple_ip_checksum( a_ip_packet, a_ip_packet_size);
        //ip_hdr->check = htons( ip_checksum );
        return 0;
    }

#ifdef __cplusplus
}
#endif

