#
# IMPORTANT: Always keep the first 7 lines (comments),
# even if this file is otherwise empty.
#
# This test file includes tests which point out known bugs.
# So all tests will cause failure.
#

if tty = STDOUT.tty?
  STDOUT.print "00"
  STDOUT.flush
end
100.times do |cnt|
  60.times.map do |i|
    Thread.new do
      x = i.to_s
      s1, s2 = IO.pipe
      t2 = Thread.new { 100.times { s2 << (x * 1000) } }
      t = Thread.new { loop { s1.getc } }
      Thread.new { sleep 0.1; s1.close }.join
      # sleep 0.1; s1.close
      begin
        t.join
      rescue IOError
      end
      # print "."
    end
  end.map(&:join)
  # print "\n"
  if tty
    STDOUT.printf "\b\b\%.2d", cnt
    STDOUT.flush
  end
end
if tty
  STDOUT.print "\b\b"
  STDOUT.flush
end
