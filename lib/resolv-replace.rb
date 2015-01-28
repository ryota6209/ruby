require 'socket'
require 'resolv'

Module.new do
  def getaddress(host)
    begin
      return Resolv.getaddress(host).to_s
    rescue Resolv::ResolvError
      raise SocketError, "Hostname not known: #{host}"
    end
  end
  IPSocket.singleton_class.__send__(:prepend, self)
end

Module.new do
  def initialize(host, serv, *rest)
    rest[0] = IPSocket.getaddress(rest[0]) if rest[0]
    super(IPSocket.getaddress(host), serv, *rest)
  end
  TCPSocket.__send__(:prepend, self)
end

Module.new do
  def bind(host, port)
    host = IPSocket.getaddress(host) if host != ""
    super(host, port)
  end

  def connect(host, port)
    super(IPSocket.getaddress(host), port)
  end

  def send(mesg, flags, *rest)
    if rest.length == 2
      host, port = rest
      begin
        addrs = Resolv.getaddresses(host)
      rescue Resolv::ResolvError
        raise SocketError, "Hostname not known: #{host}"
      end
      addrs[0...-1].each {|addr|
        begin
          return super(mesg, flags, addr, port)
        rescue SystemCallError
        end
      }
      super(mesg, flags, addrs[-1], port)
    else
      super(mesg, flags, *rest)
    end
  end
  UDPSocket.__send__(:prepend, self)
end

Module.new do
  def initialize(host, serv)
    super(IPSocket.getaddress(host), port)
  end
  SOCKSSocket.__send__(:prepend, self)
end if defined? SOCKSSocket
