
#!/usr/bin/env ruby

require 'vizkit'
require 'test/unit'
Orocos.initialize

Vizkit.logger.level = Logger::INFO

class TestWidget < Qt::Object
    attr_accessor :sample

    def update(data,port_name)
        @sample = data        
    end
end
    
class LoaderUiTest < Test::Unit::TestCase
    def setup
        Vizkit::ReaderWriterProxy.default_policy[:port_proxy] = Vizkit::TaskProxy.new("port_proxy")
        Vizkit.use_tasks([])
        #generate log file 
        @log_path = File.join(File.dirname(__FILE__),"test_log")
        if !File.exist?(@log_path+".0.log")
            output = Pocolog::Logfiles.create(@log_path,Orocos.registry)
            Orocos.load_typekit_for("/base/Time")
            stream_output = output.stream("test_task.time","/base/Time",true)

            time = Time.now
            0.upto 100 do |i|
                stream_output.write(time+i,time+i,time+i)
            end
            output.close
        end
    end

    #test integration between Vizkit, TaskProxy and Replay
    def test_1_vizkit_log_replay
        #open log file
        log = Orocos::Log::Replay.open(@log_path+".0.log")
        assert(log)

        Vizkit.connect_port_to "test_task","time" do |sample,_|
            @sample = sample
        end

        #create TaskProxy
        task = Vizkit::TaskProxy.new("test_task")
        assert(task)
        port = task.port("time")
        assert(port)
        reader = port.reader
        assert(reader)

        assert(!reader.read)
        assert(!reader.__valid?)
        assert(!task.reachable?)

        #Register task
        Vizkit.use_tasks log.tasks

        #test type of the underlying objects
        assert(task.__task.is_a?(Orocos::Log::TaskContext))
        assert(port.task.__task.is_a?(Orocos::Log::TaskContext))
        assert(port.__port.is_a?(Orocos::Log::OutputPort))
        #test if task is reachable now

        assert(task.reachable?)
        assert(port.task.reachable?)
        assert(!reader.__valid?)
        assert(reader.__reader_writer)
        assert(reader.__valid?)

        #start replay 
        sleep(0.2)
        while $qApp.hasPendingEvents
            $qApp.processEvents
        end
        log.step
        assert(reader.read)
        sleep(0.2)
        while $qApp.hasPendingEvents
            $qApp.processEvents
        end
        assert(@sample)
    end

    def test_vizkit_display
        log = Orocos::Log::Replay.open(@log_path+".0.log")
        assert(log)
        task = Vizkit::ReaderWriterProxy.default_policy[:port_proxy]

        Orocos.run "rock_port_proxy" do 
            task.start

            assert(task.createProxyConnection("test","/base/Time",0.01,true))
            assert(task.has_port? "in_test")
            
            widget = Vizkit.display task.in_test
            assert(widget)
            widget.close

            widget = Vizkit.display log.test_task.time
            assert(widget)
            widget.close

            task2 = Vizkit::TaskProxy.new("test_task")
            widget = Vizkit.display task2.port("time")
            assert(!widget)
            Vizkit.use_tasks log.tasks
            widget = Vizkit.display task2.time
            assert(widget)
            widget.close
        end
    end

    def test_vizkit_control
        task = Vizkit::ReaderWriterProxy.default_policy[:port_proxy]

        Orocos.run "rock_port_proxy" do 
            task.start

            assert(task.createProxyConnection("test","/base/Angle",0.01,true))
            assert(task.has_port? "in_test")
            
            widget = Vizkit.control task.out_test
            assert(widget)
            widget.close

            widget = Vizkit.control task.out_test.type_name
            assert(widget)
            widget.close

            widget = Vizkit.control task.out_test.new_sample
            assert(widget)
            widget.close
        end
        #process events otherwise qt is crashing
        sleep(0.2)
        while $qApp.hasPendingEvents
            $qApp.processEvents
        end
    end

    def test_vizkit_connect_port_to
        puts "###########################"
        log = Orocos::Log::Replay.open(@log_path+".0.log")
        Vizkit.use_tasks log.tasks
        assert(log)
        time = nil
        Vizkit.connect_port_to("test_task","time") do |sample, _|
            time = sample
            puts 123
        end
        log.step
        sleep(0.5)
        while $qApp.hasPendingEvents
            $qApp.processEvents
        end
        assert(time)
        #test connect_port_to with an orocos task
    end

    def test_vizkit_disconnect

    end
end