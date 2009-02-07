#include "rubysocket.h"

#if defined(HAVE_ST_MSG_CONTROL)
static VALUE rb_cAncillaryData;

static VALUE
constant_to_sym(int constant, ID (*intern_const)(int))
{
    ID name = intern_const(constant);
    if (name) {
        return ID2SYM(name);
    }

    return INT2NUM(constant);
}

static VALUE
cmsg_type_to_sym(int level, int cmsg_type)
{
    switch (level) {
      case SOL_SOCKET:
        return constant_to_sym(cmsg_type, intern_scm_optname);
      case IPPROTO_IP:
        return constant_to_sym(cmsg_type, intern_ip_optname);
#ifdef IPPROTO_IPV6
      case IPPROTO_IPV6:
        return constant_to_sym(cmsg_type, intern_ipv6_optname);
#endif
      case IPPROTO_TCP:
        return constant_to_sym(cmsg_type, intern_tcp_optname);
      case IPPROTO_UDP:
        return constant_to_sym(cmsg_type, intern_udp_optname);
      default:
        return INT2NUM(cmsg_type);
    }
}

/*
 * call-seq:
 *   Socket::AncillaryData.new(cmsg_level, cmsg_type, cmsg_data) -> ancillarydata
 *
 * _cmsg_level_ should be an integer, a string or a symbol.
 * - Socket::SOL_SOCKET, "SOL_SOCKET", "SOCKET", :SOL_SOCKET and :SOCKET
 * - Socket::IPPROTO_IP, "IP" and :IP
 * - Socket::IPPROTO_IPV6, "IPV6" and :IPV6
 * - etc.
 *
 * _cmsg_type_ should be an integer, a string or a symbol.
 * If a string/symbol is specified, it is interepreted depend on _cmsg_level_.
 * - Socket::SCM_RIGHTS, "SCM_RIGHTS", "RIGHTS", :SCM_RIGHTS, :RIGHTS for SOL_SOCKET
 * - Socket::IP_RECVTTL, "RECVTTL" and :RECVTTL for IPPROTO_IP
 * - Socket::IPV6_PKTINFO, "PKTINFO" and :PKTINFO for IPPROTO_IPV6
 * - etc.
 *
 * _cmsg_data_ should be a string.
 *
 *   p Socket::AncillaryData.new(:IPV6, :PKTINFO, "")     
 *   #=> #<Socket::AncillaryData: IPV6 PKTINFO "">
 *
 */
static VALUE
ancillary_initialize(VALUE self, VALUE vlevel, VALUE vtype, VALUE data)
{
    int level;
    StringValue(data);
    level = level_arg(vlevel);
    rb_ivar_set(self, rb_intern("level"), INT2NUM(level));
    rb_ivar_set(self, rb_intern("type"), INT2NUM(cmsg_type_arg(level, vtype)));
    rb_ivar_set(self, rb_intern("data"), data);
    return self;
}

static VALUE
ancdata_new(int level, int type, VALUE data)
{
    NEWOBJ(obj, struct RObject);
    OBJSETUP(obj, rb_cAncillaryData, T_OBJECT);
    StringValue(data);
    ancillary_initialize((VALUE)obj, INT2NUM(level), INT2NUM(type), data);
    return (VALUE)obj;
}

static int
ancillary_level(VALUE self)
{
    VALUE v = rb_attr_get(self, rb_intern("level"));
    return NUM2INT(v);
}

/*
 * call-seq:
 *   ancillarydata.level => integer
 *
 * returns the cmsg level as an integer.
 *
 *   p Socket::AncillaryData.new(:IPV6, :PKTINFO, "").level
 *   #=> 41
 */
static VALUE
ancillary_level_m(VALUE self)
{
    return INT2NUM(ancillary_level(self));
}

static int
ancillary_type(VALUE self)
{
    VALUE v = rb_attr_get(self, rb_intern("type"));
    return NUM2INT(v);
}

/*
 * call-seq:
 *   ancillarydata.type => integer
 *
 * returns the cmsg type as an integer.
 *
 *   p Socket::AncillaryData.new(:IPV6, :PKTINFO, "").type
 *   #=> 2
 */
static VALUE
ancillary_type_m(VALUE self)
{
    return INT2NUM(ancillary_type(self));
}

/*
 * call-seq:
 *   ancillarydata.data => string
 *
 * returns the cmsg data as a string.
 *
 *   p Socket::AncillaryData.new(:IPV6, :PKTINFO, "").data
 *   #=> ""
 */
static VALUE
ancillary_data(VALUE self)
{
    VALUE v = rb_attr_get(self, rb_intern("data"));
    StringValue(v);
    return v;
}


/*
 * call-seq:
 *   Socket::AncillaryData.int(cmsg_level, cmsg_type, integer) => ancillarydata
 *
 * Creates a new Socket::AncillaryData object which contains a int as data.
 *
 * The size and endian is dependent on the host. 
 *
 *   p Socket::AncillaryData.int(:SOCKET, :RIGHTS, STDERR.fileno)
 *   #=> #<Socket::AncillaryData: SOCKET RIGHTS 2>
 */
static VALUE
ancillary_s_int(VALUE klass, VALUE vlevel, VALUE vtype, VALUE integer)
{
    int level = level_arg(vlevel);
    int type = cmsg_type_arg(level, vtype);
    int i = NUM2INT(integer);
    return ancdata_new(level, type, rb_str_new((char*)&i, sizeof(i)));
}

/*
 * call-seq:
 *   ancillarydata.int => integer
 *
 * Returns the data in _ancillarydata_ as an int.
 *
 * The size and endian is dependent on the host. 
 *
 *   ancdata = Socket::AncillaryData.int(:SOCKET, :RIGHTS, STDERR.fileno)
 *   p ancdata.int => 2
 */
static VALUE
ancillary_int(VALUE self)
{
    VALUE data;
    int i;
    data = ancillary_data(self);
    if (RSTRING_LEN(data) != sizeof(int))
        rb_raise(rb_eTypeError, "size differ.  expected as sizeof(int)=%d but %ld", (int)sizeof(int), (long)RSTRING_LEN(data));
    memcpy((char*)&i, RSTRING_PTR(data), sizeof(int));
    return INT2NUM(i);
}

static VALUE
ancillary_s_ip_pktinfo(VALUE self, VALUE v_addr, VALUE v_ifindex, VALUE v_spec_dst)
{
#if defined(IPPROTO_IP) && defined(IP_PKTINFO) && defined(HAVE_TYPE_STRUCT_IN_PKTINFO) /* GNU/Linux */
    unsigned int ifindex;
    struct sockaddr_in sa;
    struct in_pktinfo pktinfo;

    SockAddrStringValue(v_addr);
    ifindex = NUM2UINT(v_ifindex);
    SockAddrStringValue(v_spec_dst);

    memset(&pktinfo, 0, sizeof(pktinfo));

    memset(&sa, 0, sizeof(sa));
    if (RSTRING_LEN(v_addr) != sizeof(sa))
        rb_raise(rb_eArgError, "addr size different to AF_INET sockaddr");
    memcpy(&sa, RSTRING_PTR(v_addr), sizeof(sa));
    if (sa.sin_family != AF_INET)
        rb_raise(rb_eArgError, "addr is not AF_INET sockaddr");
    memcpy(&pktinfo.ipi_addr, &sa.sin_addr, sizeof(pktinfo.ipi_addr));

    pktinfo.ipi_ifindex = ifindex;

    memset(&sa, 0, sizeof(sa));
    if (RSTRING_LEN(v_spec_dst) != sizeof(sa))
        rb_raise(rb_eArgError, "spec_dat size different to AF_INET sockaddr");
    memcpy(&sa, RSTRING_PTR(v_spec_dst), sizeof(sa));
    if (sa.sin_family != AF_INET)
        rb_raise(rb_eArgError, "spec_dst is not AF_INET sockaddr");
    memcpy(&pktinfo.ipi_spec_dst, &sa.sin_addr, sizeof(pktinfo.ipi_spec_dst));

    return ancdata_new(IPPROTO_IP, IP_PKTINFO, rb_str_new((char *)&pktinfo, sizeof(pktinfo)));
#else
    rb_notimplement();
#endif
}

static VALUE
ancillary_ip_pktinfo(VALUE self)
{
#if defined(IPPROTO_IP) && defined(IP_PKTINFO) && defined(HAVE_TYPE_STRUCT_IN_PKTINFO) /* GNU/Linux */
    int level, type;
    VALUE data;
    struct in_pktinfo pktinfo;
    struct sockaddr_in sa;
    VALUE v_spec_dst, v_addr;

    level = ancillary_level(self);
    type = ancillary_type(self);
    data = ancillary_data(self);

    if (level != IPPROTO_IP || type != IP_PKTINFO ||
        RSTRING_LEN(data) != sizeof(struct in_pktinfo)) {
        rb_raise(rb_eTypeError, "IP_PKTINFO ancillary data expected");
    }

    memcpy(&pktinfo, RSTRING_PTR(data), sizeof(struct in_pktinfo));
    memset(&sa, 0, sizeof(sa));

    sa.sin_family = AF_INET;
    memcpy(&sa.sin_addr, &pktinfo.ipi_addr, sizeof(sa.sin_addr));
    v_addr = addrinfo_new((struct sockaddr *)&sa, sizeof(sa), PF_INET, 0, 0, Qnil, Qnil);

    sa.sin_family = AF_INET;
    memcpy(&sa.sin_addr, &pktinfo.ipi_spec_dst, sizeof(sa.sin_addr));
    v_spec_dst = addrinfo_new((struct sockaddr *)&sa, sizeof(sa), PF_INET, 0, 0, Qnil, Qnil);

    return rb_ary_new3(3, v_addr, UINT2NUM(pktinfo.ipi_ifindex), v_spec_dst);
#else
    rb_notimplement();
#endif
}

static VALUE
ancillary_s_ipv6_pktinfo(VALUE self, VALUE v_addr, VALUE v_ifindex)
{
#if defined(IPPROTO_IPV6) && defined(IPV6_PKTINFO) /* IPv6 RFC3542 */
    unsigned int ifindex;
    struct sockaddr_in6 sa;
    struct in6_pktinfo pktinfo;

    SockAddrStringValue(v_addr);
    ifindex = NUM2UINT(v_ifindex);

    memset(&pktinfo, 0, sizeof(pktinfo));

    memset(&sa, 0, sizeof(sa));
    if (RSTRING_LEN(v_addr) != sizeof(sa))
        rb_raise(rb_eArgError, "addr size different to AF_INET6 sockaddr");
    memcpy(&sa, RSTRING_PTR(v_addr), sizeof(sa));
    if (sa.sin6_family != AF_INET6)
        rb_raise(rb_eArgError, "addr is not AF_INET6 sockaddr");
    memcpy(&pktinfo.ipi6_addr, &sa.sin6_addr, sizeof(pktinfo.ipi6_addr));

    pktinfo.ipi6_ifindex = ifindex;

    return ancdata_new(IPPROTO_IPV6, IPV6_PKTINFO, rb_str_new((char *)&pktinfo, sizeof(pktinfo)));
#else
    rb_notimplement();
#endif
}

#if defined(IPPROTO_IPV6) && defined(IPV6_PKTINFO) /* IPv6 RFC3542 */
static void
extract_ipv6_pktinfo(VALUE self, struct in6_pktinfo *pktinfo_ptr, struct sockaddr_in6 *sa_ptr)
{
    int level, type;
    VALUE data;

    level = ancillary_level(self);
    type = ancillary_type(self);
    data = ancillary_data(self);

    if (level != IPPROTO_IPV6 || type != IPV6_PKTINFO ||
        RSTRING_LEN(data) != sizeof(struct in6_pktinfo)) {
        rb_raise(rb_eTypeError, "IPV6_PKTINFO ancillary data expected");
    }

    memcpy(pktinfo_ptr, RSTRING_PTR(data), sizeof(*pktinfo_ptr));

    memset(sa_ptr, 0, sizeof(*sa_ptr));
    sa_ptr->sin6_family = AF_INET6;
    memcpy(&sa_ptr->sin6_addr, &pktinfo_ptr->ipi6_addr, sizeof(sa_ptr->sin6_addr));
    if (IN6_IS_ADDR_LINKLOCAL(&sa_ptr->sin6_addr))
        sa_ptr->sin6_scope_id = pktinfo_ptr->ipi6_ifindex;
}
#endif

static VALUE
ancillary_ipv6_pktinfo(VALUE self)
{
#if defined(IPPROTO_IPV6) && defined(IPV6_PKTINFO) /* IPv6 RFC3542 */
    struct in6_pktinfo pktinfo;
    struct sockaddr_in6 sa;
    VALUE v_addr;

    extract_ipv6_pktinfo(self, &pktinfo, &sa);
    v_addr = addrinfo_new((struct sockaddr *)&sa, sizeof(sa), PF_INET6, 0, 0, Qnil, Qnil);
    return rb_ary_new3(2, v_addr, UINT2NUM(pktinfo.ipi6_ifindex));
#else
    rb_notimplement();
#endif
}

static VALUE
ancillary_ipv6_pktinfo_addr(VALUE self)
{
#if defined(IPPROTO_IPV6) && defined(IPV6_PKTINFO) /* IPv6 RFC3542 */
    struct in6_pktinfo pktinfo;
    struct sockaddr_in6 sa;
    extract_ipv6_pktinfo(self, &pktinfo, &sa);
    return addrinfo_new((struct sockaddr *)&sa, sizeof(sa), PF_INET6, 0, 0, Qnil, Qnil);
#else
    rb_notimplement();
#endif
}

static VALUE
ancillary_ipv6_pktinfo_ifindex(VALUE self)
{
#if defined(IPPROTO_IPV6) && defined(IPV6_PKTINFO) /* IPv6 RFC3542 */
    struct in6_pktinfo pktinfo;
    struct sockaddr_in6 sa;
    extract_ipv6_pktinfo(self, &pktinfo, &sa);
    return UINT2NUM(pktinfo.ipi6_ifindex);
#else
    rb_notimplement();
#endif
}

#if defined(SOL_SOCKET) && defined(SCM_RIGHTS) /* 4.4BSD */
static int
anc_inspect_socket_rights(int level, int type, VALUE data, VALUE ret)
{
    if (level == SOL_SOCKET && type == SCM_RIGHTS &&
        0 < RSTRING_LEN(data) && (RSTRING_LEN(data) % sizeof(int) == 0)) {
        long off;
        for (off = 0; off < RSTRING_LEN(data); off += sizeof(int)) {
            int fd;
            memcpy((char*)&fd, RSTRING_PTR(data)+off, sizeof(int));
            rb_str_catf(ret, " %d", fd);
        }
        return 0;
    }
    else {
        return -1;
    }
}
#endif

#if defined(IPPROTO_IP) && defined(IP_RECVDSTADDR) /* 4.4BSD */
static int
anc_inspect_ip_recvdstaddr(int level, int type, VALUE data, VALUE ret)
{
    if (level == IPPROTO_IP && type == IP_RECVDSTADDR &&
        RSTRING_LEN(data) == sizeof(struct in_addr)) {
        struct in_addr addr; 
        char addrbuf[INET_ADDRSTRLEN];
        memcpy(&addr, RSTRING_PTR(data), sizeof(addr));
        if (inet_ntop(AF_INET, &addr, addrbuf, sizeof(addrbuf)) == NULL)
            rb_str_cat2(ret, " invalid-address");
        else
            rb_str_catf(ret, " %s", addrbuf);
        return 0;
    }
    else {
        return -1;
    }
}
#endif

#if defined(IPPROTO_IP) && defined(IP_PKTINFO) && defined(HAVE_TYPE_STRUCT_IN_PKTINFO) /* GNU/Linux */
static int
anc_inspect_ip_pktinfo(int level, int type, VALUE data, VALUE ret)
{
    if (level == IPPROTO_IP && type == IP_PKTINFO &&
        RSTRING_LEN(data) == sizeof(struct in_pktinfo)) {
        struct in_pktinfo pktinfo;
        char buf[INET_ADDRSTRLEN > IFNAMSIZ ? INET_ADDRSTRLEN : IFNAMSIZ];
        memcpy(&pktinfo, RSTRING_PTR(data), sizeof(pktinfo));
        if (inet_ntop(AF_INET, &pktinfo.ipi_addr, buf, sizeof(buf)) == NULL)
            rb_str_cat2(ret, " addr:invalid-address");
        else
            rb_str_catf(ret, " addr:%s", buf);
        if (if_indextoname(pktinfo.ipi_ifindex, buf) == NULL)
            rb_str_catf(ret, " ifindex:%d", pktinfo.ipi_ifindex);
        else
            rb_str_catf(ret, " %s", buf);
        if (inet_ntop(AF_INET, &pktinfo.ipi_spec_dst, buf, sizeof(buf)) == NULL)
            rb_str_cat2(ret, " spec_dst:invalid-address");
        else
            rb_str_catf(ret, " spec_dst:%s", buf);
        return 0;
    }
    else {
        return -1;
    }
}
#endif

#if defined(IPPROTO_IPV6) && defined(IPV6_PKTINFO) && defined(HAVE_TYPE_STRUCT_IN6_PKTINFO) /* IPv6 RFC3542 */
static int
anc_inspect_ipv6_pktinfo(int level, int type, VALUE data, VALUE ret)
{
    if (level == IPPROTO_IPV6 && type == IPV6_PKTINFO &&
        RSTRING_LEN(data) == sizeof(struct in6_pktinfo)) {
        struct in6_pktinfo *pktinfo = (struct in6_pktinfo *)RSTRING_PTR(data);
        struct in6_addr addr; 
        unsigned int ifindex;
        char addrbuf[INET6_ADDRSTRLEN], ifbuf[IFNAMSIZ];
        memcpy(&addr, &pktinfo->ipi6_addr, sizeof(addr));
        memcpy(&ifindex, &pktinfo->ipi6_ifindex, sizeof(ifindex));
        if (inet_ntop(AF_INET6, &addr, addrbuf, sizeof(addrbuf)) == NULL)
            rb_str_cat2(ret, " invalid-address");
        else
            rb_str_catf(ret, " %s", addrbuf);
        if (if_indextoname(ifindex, ifbuf) == NULL)
            rb_str_catf(ret, " ifindex:%d", ifindex);
        else
            rb_str_catf(ret, " %s", ifbuf);
        return 0;
    }
    else {
        return -1;
    }
}
#endif

/*
 * call-seq:
 *   ancillarydata.inspect => string
 *
 * returns a string which shows ancillarydata in human-readable form.
 *
 *   Socket::AncillaryData.new(:IPV6, :PKTINFO, "").inspect
 *   #=> #<Socket::AncillaryData: IPV6 PKTINFO "">
 */
static VALUE
ancillary_inspect(VALUE self)
{
    VALUE ret;
    int level, type;
    VALUE data;
    ID level_id;
    VALUE vtype;

    level = ancillary_level(self);
    type = ancillary_type(self);
    data = ancillary_data(self);

    ret = rb_sprintf("#<%s: ", rb_obj_classname(self));

    level_id = intern_level(level);
    if (level_id)
        rb_str_cat2(ret, rb_id2name(level_id));
    else
        rb_str_catf(ret, "cmsg_level:%d", level);

    vtype = cmsg_type_to_sym(level, type);
    if (SYMBOL_P(vtype))
        rb_str_catf(ret, " %s", rb_id2name(SYM2ID(vtype)));
    else
        rb_str_catf(ret, " cmsg_type:%d", type);

    switch (level) {
#    if defined(SOL_SOCKET)
      case SOL_SOCKET:
        switch (type) {
#        if defined(SCM_RIGHTS) /* 4.4BSD */
          case SCM_RIGHTS: if (anc_inspect_socket_rights(level, type, data, ret) == -1) goto dump; break;
#        endif
          default: goto dump;
        }
        break;
#    endif

#    if defined(IPPROTO_IP)
      case IPPROTO_IP:
        switch (type) {
#        if defined(IP_RECVDSTADDR) /* 4.4BSD */
          case IP_RECVDSTADDR: if (anc_inspect_ip_recvdstaddr(level, type, data, ret) == -1) goto dump; break;
#        endif
#        if defined(IP_PKTINFO) && defined(HAVE_TYPE_STRUCT_IN_PKTINFO) /* GNU/Linux */
          case IP_PKTINFO: if (anc_inspect_ip_pktinfo(level, type, data, ret) == -1) goto dump; break;
#        endif
          default: goto dump;
        }
        break;
#    endif

#    if defined(IPPROTO_IPV6)
      case IPPROTO_IPV6:
        switch (type) {
#        if defined(IPV6_PKTINFO) /* RFC 3542 */
          case IPV6_PKTINFO: if (anc_inspect_ipv6_pktinfo(level, type, data, ret) == -1) goto dump; break;
#        endif
          default: goto dump;
        }
        break;
#    endif

      default:
      dump:
        data = rb_str_dump(data);
        rb_str_catf(ret, " %s", StringValueCStr(data));
    }

    rb_str_cat2(ret, ">");

    return ret;
}

/*
 * call-seq:
 *   ancillarydata.cmsg_is?(level, type) => true or false
 *
 * tests the level and type of _ancillarydata_.
 *
 *   ancdata = Socket::AncillaryData.new(:IPV6, :PKTINFO, "")
 *   ancdata.cmsg_is?(Socket::IPPROTO_IPV6, Socket::IPV6_PKTINFO) #=> true
 *   ancdata.cmsg_is?(:IPV6, :PKTINFO)       #=> true
 *   ancdata.cmsg_is?(:IP, :PKTINFO)         #=> false
 *   ancdata.cmsg_is?(:SOCKET, :RIGHTS)      #=> false
 */
static VALUE
ancillary_cmsg_is_p(VALUE self, VALUE vlevel, VALUE vtype)
{
    int level = level_arg(vlevel);
    int type = cmsg_type_arg(level, vtype);

    if (ancillary_level(self) == level &&
        ancillary_type(self) == type)
        return Qtrue;
    else
        return Qfalse;
}

#endif

#if defined(HAVE_SENDMSG)
struct sendmsg_args_struct {
    int fd;
    const struct msghdr *msg;
    int flags;
};

static VALUE
nogvl_sendmsg_func(void *ptr)
{
    struct sendmsg_args_struct *args = ptr;
    return sendmsg(args->fd, args->msg, args->flags);
}

static ssize_t
rb_sendmsg(int fd, const struct msghdr *msg, int flags)
{
    struct sendmsg_args_struct args;
    args.fd = fd;
    args.msg = msg;
    args.flags = flags;
    return rb_thread_blocking_region(nogvl_sendmsg_func, &args, RUBY_UBF_IO, 0);
}

static VALUE
bsock_sendmsg_internal(int argc, VALUE *argv, VALUE sock, int nonblock)
{
    rb_io_t *fptr;
    VALUE data, vflags, dest_sockaddr;
    VALUE *controls_ptr;
    int controls_num;
    struct msghdr mh;
    struct iovec iov;
#if defined(HAVE_ST_MSG_CONTROL)
    volatile VALUE controls_str = 0;
#endif
    int flags;
    ssize_t ss;

    rb_secure(4);

    data = vflags = dest_sockaddr = Qnil;
    controls_ptr = NULL;
    controls_num = 0;

    if (argc == 0)
        rb_raise(rb_eArgError, "mesg argument required");
    data = argv[0];
    if (1 < argc) vflags = argv[1];
    if (2 < argc) dest_sockaddr = argv[2];
    if (3 < argc) { controls_ptr = &argv[3]; controls_num = argc - 3; }

    StringValue(data);

    if (controls_num) {
#if defined(HAVE_ST_MSG_CONTROL)
	int i;
	int last_pad = 0;
        controls_str = rb_str_tmp_new(0);
        for (i = 0; i < controls_num; i++) {
            VALUE elt = controls_ptr[i], v;
            VALUE vlevel, vtype;
            int level, type;
            VALUE cdata;
            long oldlen;
            struct cmsghdr *cmh;
            size_t cspace;
            v = rb_check_convert_type(elt, T_ARRAY, "Array", "to_ary");
            if (!NIL_P(v)) {
                elt = v;
                if (RARRAY_LEN(elt) != 3)
                    rb_raise(rb_eArgError, "an element of controls should be 3-elements array");
                vlevel = rb_ary_entry(elt, 0);
                vtype = rb_ary_entry(elt, 1);
                cdata = rb_ary_entry(elt, 2);
            }
            else {
                vlevel = rb_funcall(elt, rb_intern("level"), 0);
                vtype = rb_funcall(elt, rb_intern("type"), 0);
                cdata = rb_funcall(elt, rb_intern("data"), 0);
            }
            level = level_arg(vlevel);
            type = cmsg_type_arg(level, vtype);
            StringValue(cdata);
            oldlen = RSTRING_LEN(controls_str);
            cspace = CMSG_SPACE(RSTRING_LEN(cdata));
            rb_str_resize(controls_str, oldlen + cspace);
            cmh = (struct cmsghdr *)(RSTRING_PTR(controls_str)+oldlen);
            memset((char *)cmh, 0, cspace);
            cmh->cmsg_level = level;
            cmh->cmsg_type = type;
            cmh->cmsg_len = CMSG_LEN(RSTRING_LEN(cdata));
            MEMCPY(CMSG_DATA(cmh), RSTRING_PTR(cdata), char, RSTRING_LEN(cdata));
	    last_pad = cspace - cmh->cmsg_len;
        }
	if (last_pad) {
	    rb_str_set_len(controls_str, RSTRING_LEN(controls_str)-last_pad);
	}
#else
	rb_raise(rb_eNotImpError, "control message for sendmsg is unimplemented");
#endif
    }

    flags = NIL_P(vflags) ? 0 : NUM2INT(vflags);
#ifdef MSG_DONTWAIT
    if (nonblock)
        flags |= MSG_DONTWAIT;
#endif

    if (!NIL_P(dest_sockaddr))
	SockAddrStringValue(dest_sockaddr);

    GetOpenFile(sock, fptr);

  retry:
    memset(&mh, 0, sizeof(mh));
    if (!NIL_P(dest_sockaddr)) {
        mh.msg_name = RSTRING_PTR(dest_sockaddr);
        mh.msg_namelen = RSTRING_LEN(dest_sockaddr);
    }
    mh.msg_iovlen = 1;
    mh.msg_iov = &iov;
    iov.iov_base = RSTRING_PTR(data);
    iov.iov_len = RSTRING_LEN(data);
#if defined(HAVE_ST_MSG_CONTROL)
    if (controls_str) {
        mh.msg_control = RSTRING_PTR(controls_str);
        mh.msg_controllen = RSTRING_LEN(controls_str);
    }
    else {
        mh.msg_control = NULL;
        mh.msg_controllen = 0;
    }
#endif

    rb_io_check_closed(fptr);
    if (nonblock)
        rb_io_set_nonblock(fptr);

    ss = rb_sendmsg(fptr->fd, &mh, flags);

    if (!nonblock && rb_io_wait_writable(fptr->fd)) {
        rb_io_check_closed(fptr);
        goto retry;
    }

    if (ss == -1)
	rb_sys_fail("sendmsg(2)");

    return SSIZET2NUM(ss);
}
#else
static VALUE
bsock_sendmsg_internal(int argc, VALUE *argv, VALUE sock, int nonblock)
{
    rb_notimplement();
}
#endif

/*
 * call-seq:
 *    basicsocket.sendmsg(mesg, flags=0, dest_sockaddr=nil, *controls) => sent_len
 *
 * sendmsg sends a message using sendmsg(2) system call in blocking manner.
 *
 * _mesg_ is a string to send.
 *
 * _flags_ is bitwise OR of MSG_* constants such as Socket::MSG_OOB.
 *
 * _dest_sockaddr_ is a destination socket address for connection-less socket.
 * It should be a sockaddr such as a result of Socket.sockaddr_in.
 * An Addrinfo object can be used too.
 *
 * _controls_ is a list of ancillary data.
 * The element of _controls_ should be Socket::AncillaryData or
 * 3-elements array.
 * The 3-element array should contains cmsg_level, cmsg_type and data.
 *
 * The return value, _sent_len_, is an integer which is the number of bytes sent.
 *
 * sendmsg can be used to implement send_io as follows:
 *
 *   # use Socket::AncillaryData.
 *   ancdata = Socket::AncillaryData.int(:SOCKET, :RIGHTS, io.fileno)       
 *   sock.sendmsg("a", 0, nil, ancdata)
 *
 *   # use 3-element array.
 *   ancdata = [:SOCKET, :RIGHTS, [io.fileno].pack("i!")]
 *   sock.sendmsg("\0", 0, nil, ancdata)
 *
 */
static VALUE
bsock_sendmsg(int argc, VALUE *argv, VALUE sock)
{
    return bsock_sendmsg_internal(argc, argv, sock, 0);
}

/*
 * call-seq:
 *    basicsocket.sendmsg_nonblock(mesg, flags=0, dest_sockaddr=nil, *controls) => sent_len
 *
 * sendmsg_nonblock sends a message using sendmsg(2) system call in non-blocking manner.
 *
 * It is similar to BasicSocket#sendmsg
 * but the non-blocking flag is set before the system call
 * and it doesn't retry the system call.
 *
 */
static VALUE
bsock_sendmsg_nonblock(int argc, VALUE *argv, VALUE sock)
{
    return bsock_sendmsg_internal(argc, argv, sock, 1);
}

#if defined(HAVE_RECVMSG)
struct recvmsg_args_struct {
    int fd;
    struct msghdr *msg;
    int flags;
};

static VALUE
nogvl_recvmsg_func(void *ptr)
{
    struct recvmsg_args_struct *args = ptr;
    return recvmsg(args->fd, args->msg, args->flags);
}

static ssize_t
rb_recvmsg(int fd, struct msghdr *msg, int flags)
{
    struct recvmsg_args_struct args;
    args.fd = fd;
    args.msg = msg;
    args.flags = flags;
    return rb_thread_blocking_region(nogvl_recvmsg_func, &args, RUBY_UBF_IO, 0);
}

static VALUE
bsock_recvmsg_internal(int argc, VALUE *argv, VALUE sock, int nonblock)
{
    rb_io_t *fptr;
    VALUE vmaxdatlen, vmaxctllen, vflags;
    int grow_buffer;
    size_t maxdatlen, maxctllen;
    int flags, orig_flags;
    struct msghdr mh;
    struct iovec iov;
#if defined(HAVE_ST_MSG_CONTROL)
    struct cmsghdr *cmh;
#endif
    char namebuf[1024];
    char datbuf0[4096], *datbuf;
    char ctlbuf0[4096], *ctlbuf;
    VALUE dat_str = Qnil;
    VALUE ctl_str = Qnil;
    VALUE ret;
    ssize_t ss;

    rb_secure(4);

    rb_scan_args(argc, argv, "03", &vmaxdatlen, &vflags, &vmaxctllen);

    maxdatlen = NIL_P(vmaxdatlen) ? sizeof(datbuf0) : NUM2SIZET(vmaxdatlen);
    maxctllen = NIL_P(vmaxctllen) ? sizeof(ctlbuf0) : NUM2SIZET(vmaxctllen);
    flags = NIL_P(vflags) ? 0 : NUM2INT(vflags);
#ifdef MSG_DONTWAIT
    if (nonblock)
        flags |= MSG_DONTWAIT;
#endif
    orig_flags = flags;

    grow_buffer = NIL_P(vmaxdatlen) || NIL_P(vmaxctllen);

    GetOpenFile(sock, fptr);
    if (rb_io_read_pending(fptr)) {
        rb_raise(rb_eIOError, "recvfrom for buffered IO");
    }

#if !defined(HAVE_ST_MSG_CONTROL)
    if (grow_buffer) {
	int socktype, optlen = sizeof(socktype);
        if (getsockopt(fptr->fd, SOL_SOCKET, SO_TYPE, (void*)&socktype, &optlen) == -1) {
	    rb_sys_fail("getsockopt(SO_TYPE)");
	}
	if (socktype == SOCK_STREAM)
	    grow_buffer = 0;
    }
#endif

  retry:
    if (maxdatlen <= sizeof(datbuf0))
        datbuf = datbuf0;
    else {
        if (NIL_P(dat_str))
            dat_str = rb_str_tmp_new(maxdatlen);
        else
            rb_str_resize(dat_str, maxdatlen);
        datbuf = RSTRING_PTR(dat_str);
    }

    if (maxctllen <= sizeof(ctlbuf0))
        ctlbuf = ctlbuf0;
    else {
        if (NIL_P(ctl_str))
            ctl_str = rb_str_tmp_new(maxctllen);
        else
            rb_str_resize(ctl_str, maxctllen);
        ctlbuf = RSTRING_PTR(ctl_str);
    }

    memset(&mh, 0, sizeof(mh));

    memset(namebuf, 0, sizeof(namebuf));
    mh.msg_name = namebuf;
    mh.msg_namelen = sizeof(namebuf);

    mh.msg_iov = &iov;
    mh.msg_iovlen = 1;
    iov.iov_base = datbuf;
    iov.iov_len = maxdatlen;

#if defined(HAVE_ST_MSG_CONTROL)
    mh.msg_control = ctlbuf;
    mh.msg_controllen = maxctllen;
#endif

    if (grow_buffer)
        flags |= MSG_PEEK;

    rb_io_check_closed(fptr);
    if (nonblock)
        rb_io_set_nonblock(fptr);

    ss = rb_recvmsg(fptr->fd, &mh, flags);

    if (!nonblock && rb_io_wait_readable(fptr->fd)) {
        rb_io_check_closed(fptr);
        goto retry;
    }

    if (grow_buffer) {
	int grown = 0;
#if defined(HAVE_ST_MSG_CONTROL)
        if (NIL_P(vmaxdatlen) && (mh.msg_flags & MSG_TRUNC)) {
	    maxdatlen *= 2;
	    grown = 1;
	}
        if (NIL_P(vmaxctllen) && (mh.msg_flags & MSG_CTRUNC)) {
	    maxctllen *= 2;
	    grown = 1;
	}
#else
	if (NIL_P(vmaxdatlen) && ss != -1 && ss == iov.iov_len) {
	    maxdatlen *= 2;
	    grown = 1;
	}
#endif
	if (grown) {
	    goto retry;
	}
	else {
            grow_buffer = 0;
            if (flags != orig_flags) {
                flags = orig_flags;
                goto retry;
            }
        }
    }

    if (ss == -1)
	rb_sys_fail("recvmsg(2)");

    if (NIL_P(dat_str))
        dat_str = rb_tainted_str_new(datbuf, ss);
    else {
        rb_str_resize(dat_str, ss);
        OBJ_TAINT(dat_str);
        RBASIC(dat_str)->klass = rb_cString;
    }

    ret = rb_ary_new3(3, dat_str,
                         io_socket_addrinfo(sock, mh.msg_name, mh.msg_namelen),
#if defined(HAVE_ST_MSG_CONTROL)
			 INT2NUM(mh.msg_flags)
#else
			 Qnil
#endif
			 );

#if defined(HAVE_ST_MSG_CONTROL)
    if (mh.msg_controllen) {
        for (cmh = CMSG_FIRSTHDR(&mh); cmh != NULL; cmh = CMSG_NXTHDR(&mh, cmh)) {
            VALUE ctl;
            size_t clen;
            if (cmh->cmsg_len == 0) {
                rb_raise(rb_eIOError, "invalid control message (cmsg_len == 0)");
            }
            clen = (char*)cmh + cmh->cmsg_len - (char*)CMSG_DATA(cmh);
            ctl = ancdata_new(cmh->cmsg_level, cmh->cmsg_type, rb_tainted_str_new((char*)CMSG_DATA(cmh), clen));
            rb_ary_push(ret, ctl);
        }
    }
#endif

    return ret;
}
#else
static VALUE
bsock_recvmsg_internal(int argc, VALUE *argv, VALUE sock, int nonblock)
{
    rb_notimplement();
}
#endif

/*
 * call-seq:
 *    basicsocket.recvmsg(maxmesglen=nil, flags=0, maxcontrollen=nil) => [mesg, sender_addrinfo, rflags, *controls]
 *
 * recvmsg receives a message using recvmsg(2) system call in blocking manner.
 *
 * _maxmesglen_ is the maximum length of mesg to receive.
 *
 * _flags_ is bitwise OR of MSG_* constants such as Socket::MSG_PEEK.
 *
 * _maxcontrolslen_ is the maximum length of controls (ancillary data) to receive.
 *
 * The return value is 4-elements array.
 *
 * _mesg_ is a string of the received message.
 *
 * _sender_addrinfo_ is a sender socket address for connection-less socket.
 * It is an Addrinfo object.
 * For connection-oriented socket such as TCP, sender_addrinfo is platform dependent.
 *
 * _rflags_ is a flags on the received message which is bitwise OR of MSG_* constants such as Socket::MSG_TRUNC.
 * It will be nil if the system uses 4.3BSD style old recvmsg system call.
 *
 * _controls_ is ancillary data which is an array of Socket::AncillaryData objects such as:
 *
 *   #<Socket::AncillaryData: SOCKET RIGHTS 7>
 *
 * _maxmesglen_ and _maxcontrolslen_ can be nil.
 * In that case, the buffer will be grown until the message is not truncated.
 * Internally, MSG_PEEK is used and MSG_TRUNC/MSG_CTRUNC are checked.
 *
 * sendmsg can be used to implement recv_io as follows:
 *
 *   mesg, sender_sockaddr, rflags, *controls = sock.recvmsg
 *   controls.each {|ancdata|
 *     if ancdata.level == Socket::SOL_SOCKET && ancdata.type == Socket::SCM_RIGHTS
 *       return IO.new(ancdata.int)
 *     end
 *   }
 *
 */
static VALUE
bsock_recvmsg(int argc, VALUE *argv, VALUE sock)
{
    return bsock_recvmsg_internal(argc, argv, sock, 0);
}

/*
 * call-seq:
 *    basicsocket.recvmsg_nonblock(maxdatalen=nil, flags=0, maxcontrollen=nil) => [data, sender_addrinfo, rflags, *controls]
 *
 * recvmsg receives a message using recvmsg(2) system call in non-blocking manner.
 *
 * It is similar to BasicSocket#recvmsg
 * but non-blocking flag is set before the system call
 * and it doesn't retry the system call.
 *
 */
static VALUE
bsock_recvmsg_nonblock(int argc, VALUE *argv, VALUE sock)
{
    return bsock_recvmsg_internal(argc, argv, sock, 1);
}

void
Init_ancdata(void)
{
    rb_define_method(rb_cBasicSocket, "sendmsg", bsock_sendmsg, -1);
    rb_define_method(rb_cBasicSocket, "sendmsg_nonblock", bsock_sendmsg_nonblock, -1);
    rb_define_method(rb_cBasicSocket, "recvmsg", bsock_recvmsg, -1);
    rb_define_method(rb_cBasicSocket, "recvmsg_nonblock", bsock_recvmsg_nonblock, -1);

#if defined(HAVE_ST_MSG_CONTROL)
    rb_cAncillaryData = rb_define_class_under(rb_cSocket, "AncillaryData", rb_cObject);
    rb_define_method(rb_cAncillaryData, "initialize", ancillary_initialize, 3);
    rb_define_method(rb_cAncillaryData, "inspect", ancillary_inspect, 0);
    rb_define_method(rb_cAncillaryData, "level", ancillary_level_m, 0);
    rb_define_method(rb_cAncillaryData, "type", ancillary_type_m, 0);
    rb_define_method(rb_cAncillaryData, "data", ancillary_data, 0);
    rb_define_method(rb_cAncillaryData, "cmsg_is?", ancillary_cmsg_is_p, 2);
    rb_define_singleton_method(rb_cAncillaryData, "int", ancillary_s_int, 3);
    rb_define_method(rb_cAncillaryData, "int", ancillary_int, 0);
    rb_define_singleton_method(rb_cAncillaryData, "ip_pktinfo", ancillary_s_ip_pktinfo, 3);
    rb_define_method(rb_cAncillaryData, "ip_pktinfo", ancillary_ip_pktinfo, 0);
    rb_define_singleton_method(rb_cAncillaryData, "ipv6_pktinfo", ancillary_s_ipv6_pktinfo, 2);
    rb_define_method(rb_cAncillaryData, "ipv6_pktinfo", ancillary_ipv6_pktinfo, 0);
    rb_define_method(rb_cAncillaryData, "ipv6_pktinfo_addr", ancillary_ipv6_pktinfo_addr, 0);
    rb_define_method(rb_cAncillaryData, "ipv6_pktinfo_ifindex", ancillary_ipv6_pktinfo_ifindex, 0);
#endif
}