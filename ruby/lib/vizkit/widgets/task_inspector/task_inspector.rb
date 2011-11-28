#!/usr/bin/env ruby

require 'vizkit'

require File.join(File.dirname(__FILE__), 'task_inspector_window.ui.rb')
require 'vizkit/tree_modeler'

class TaskInspector
    def self.create_widget(parent = nil)
        form = Vizkit.load(File.join(File.dirname(__FILE__),'task_inspector_window.ui'),parent)
        form.extend Functions
        form.init
        form
    end

    module Functions
        def default_options()
            options = Hash.new
            options[:interval] = 1000   #update interval in msec
            return options
        end

        def init
            buttonFrame.hide
            @brush = Qt::Brush.new(Qt::Color.new(200,200,200))
            @tree_view = Vizkit::TreeModeler.new
            @tree_view.setup_tree_view(treeView)

            @tasks = Hash.new
            @read_obj = false

            setPropButton.connect(SIGNAL('clicked()')) do
                @tree_view.update_dirty_items
                buttonFrame.hide
            end

            cancelPropButton.connect(SIGNAL('clicked()')) do
                @tree_view.unmark_dirty_items
                buttonFrame.hide
            end

            @timer = Qt::Timer.new(self)
            @timer.connect(SIGNAL('timeout()')) do 
                @tasks.each_value do |task|
                    @tree_view.update(task)
                end

                if !@tree_view.dirty_items.empty?
                    buttonFrame.show 
                else
                    treeView.resizeColumnToContents(0)
                end
            end
        end

        def config(task,options=Hash.new)
            if task.is_a? Orocos::TaskContext
                @tasks[task.name] = Vizkit::TaskProxy.new(task.name)
            elsif task.is_a? Vizkit::TaskProxy
                @tasks[task.name] = task
            end
            options = default_options.merge(options)
            @timer.start(options[:interval])
        end
    end
end

Vizkit::UiLoader.register_ruby_widget("task_inspector",TaskInspector.method(:create_widget))
Vizkit::UiLoader.register_widget_for("task_inspector",Orocos::TaskContext)
#not supported so far
#Vizkit::UiLoader.register_widget_for("task_inspector",Orocos::Log::TaskContext)
