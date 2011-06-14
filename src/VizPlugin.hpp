#ifndef __VIZKIT_VIZPLUGIN_HPP__ 
#define __VIZKIT_VIZPLUGIN_HPP__ 

#include <osg/NodeCallback>
#include <osg/Group>

#include <boost/thread/mutex.hpp>
#include <qobject.h>
#include <QDockWidget>
#include <QVariant>
#include <QtPlugin>

namespace YAML
{
    class Emitter;
    class Node;
}

namespace vizkit 
{

/** 
 * Interface class for all ruby adapters of the visualization plugins
 * Ruby adapters are usefull to get incoming data via ruby.
 */
class VizPluginRubyAdapterBase : public QObject
{
    Q_OBJECT
    
    public slots:
        virtual void update(QVariant&, bool) = 0;
        virtual QString getDataType() = 0;
        virtual QString getRubyMethod() = 0;
};

/** 
 * This class holds all ruby adapters of a specific visualization plugin.
 */
class VizPluginRubyAdapterCollection : public QObject
{
    Q_OBJECT
    
    public:
        /**
         * adds an adapter to the list of ruby adapters.
         */
        void addAdapter(VizPluginRubyAdapterBase* adapter) 
        {
            adapterList.push_back(adapter);
        };
        
        /**
         * removes an adapter from the list if available.
         */
        void removeAdapter(VizPluginRubyAdapterBase* adapter)
        {
            std::vector<VizPluginRubyAdapterBase*>::iterator it = std::find(adapterList.begin(), adapterList.end(), adapter);
            if (it != adapterList.end()) adapterList.erase(it);
        };
    public slots:
        /**
         * The method names of all available adapers will be returned.
         * @return QStringList of known adapters
         */
        QStringList* getListOfAvailableAdapter()
        {
            QStringList* adapterStringList = new QStringList();
            for(std::vector<VizPluginRubyAdapterBase*>::iterator it = adapterList.begin(); it != adapterList.end(); it++)
            {
                adapterStringList->push_back((*it)->getRubyMethod());
            }
            return adapterStringList;
        };
        
        /**
         * Retruns the ruby adapter given by its ruby method name.
         * It will be returnd as QObject, so ruby can get it.
         * @param rubyMethodName method name of the adapter
         * @return the adapter
         */
        QObject* getAdapter(QString rubyMethodName)
        {
            for(std::vector<VizPluginRubyAdapterBase*>::iterator it = adapterList.begin(); it != adapterList.end(); it++)
            {
                if ((*it)->getRubyMethod() == rubyMethodName) 
                    return *it; 
            }
            return NULL;
        };
        
    protected:
        std::vector<VizPluginRubyAdapterBase*> adapterList;
};

/** 
 * Interface class for all visualization plugins based on vizkit. All plugins
 * provide an osg::Group() node, which can be added to an osg render tree for
 * visualisation using getMainNode().
 *
 * The dirty handling works as such, that whenever the class is flagged dirty,
 * the virtual updateMainNode() function will be called when it is safe to
 * modify the node. Any plugin needs to implement this function to update the
 * visualisation. The osg node must not be modified at any other time.
 *
 * The updateMainNode() is guarded by a mutex, so it is generally a good idea to
 * guard any updates to the internal state of the plugin, that is required
 * within the updateMainNode(). Note that updateMainNode() is most likely called
 * from a different thread context than the rest.
 */
class VizPluginBase
{
    public:
        VizPluginBase();

	/** @return true if the plugins internal state has been updated */
	virtual bool isDirty() const;
	/** mark the internal state as modified */
	void setDirty();

	/** @return a pointer to the internal Group that is used to maintain the
         * plugin's nodes */
	osg::ref_ptr<osg::Group> getVizNode() const;

	/** @return the name of the plugin, it's needed to save the 
         *  configuration data in a YAML file */
	virtual const std::string getPluginName() const;

	/** override this method to save configuration data. Always call the
	 * superclass as well.
	 * @param[out] emitter object which can be used to emit yaml structure
	 *  containing configuration options
	 */
	virtual void saveData(YAML::Emitter& emitter) const {};

	/** override this method to load configuration data. Always call the
	 * superclass as well.
	 * @param[in] yamlNode object which contains previously saved
	 *  configuration options
	 */
	virtual void loadData(const YAML::Node& yamlNode) {};
        
        /**
         * @return a vector of QDockWidgets provided by this class.
         */
        std::vector<QDockWidget*> getDockWidgets();
        
        /**
         * @return an instance of the ruby adapter collection.
         */
        VizPluginRubyAdapterCollection* getRubyAdapterCollection();

    protected:
	/** override this function to update the visualisation.
	 * @param node contains a point to the node which can be modified.
	 */
	virtual void updateMainNode(osg::Node* node) = 0;

	/** override this method to provide your own main node.
	 * @return node derived from osg::Group
	 */ 
	virtual osg::ref_ptr<osg::Node> createMainNode();
        
        /** override this method to provide your own QDockWidgets.
         * The QDockWidgets will automatically attached to the main window.
         */ 
        virtual void createDockWidgets();

	/** lock this mutex outside updateMainNode if you update the internal
	 * state of the visualization.
	 */ 
	boost::mutex updateMutex;
        
        std::vector<QDockWidget*> dockWidgets;
        
        VizPluginRubyAdapterCollection adapterCollection;

    private:
	class CallbackAdapter;
	osg::ref_ptr<osg::NodeCallback> nodeCallback;
	void updateCallback(osg::Node* node);

        osg::ref_ptr<osg::Node> mainNode;
        osg::ref_ptr<osg::Group> vizNode;
	bool dirty;
};

template <typename T> class VizPlugin;

/**
 * Convenience template that adds type-specific handling to VizPluginBase
 *
 * Use this if you want a single visualization plugin to support multiple types
 * at the same time:
 *
 * <code>
 *
 *   class MyVisualizer
 *      : public VizPlugin<FirstType>,
 *      , public VizPluginAddType<SecondType>
 *   {
 *      void updateDataIntern(FirstType const&);
 *      void updateDataIntern(SecondType const&);
 *   }
 *
 */
template <typename T>
class VizPluginAddType
{
    template <typename Type> friend class VizPlugin;

    protected:
	/** overide this method and set your internal state such that the next
	 * call to updateMainNode will reflect that update.
	 * @param data data to be updated
	 */
	virtual void updateDataIntern(const T &data) = 0;
};

/** 
 * convinience class template that performs the locking of incoming data.
 * Derive from this class if you only have a single datatype to visualise, that
 * can be easily copied.
 */
template <class T>
class VizPlugin : public VizPluginBase,
    public VizPluginAddType< T >
{
    public:
	/** updates the data to be visualised and marks the visualisation dirty
	 * @param data const ref to data that is visualised
	 */
        template<typename Type>
	void updateData(const Type &data) {
	    boost::mutex::scoped_lock lockit(this->updateMutex);
	    this->setDirty();
	    VizPluginAddType<Type> *type = dynamic_cast<VizPluginAddType<Type>*>(this);
	    if(type)
		type->updateDataIntern(data);
	    else
	    {
		throw std::runtime_error("Wrong type given to visualizer");
	    }
	};
};

/** 
 * Interface class for all Vizkit Qt plugins.
 * Vizkit Qt plugins are only helper classes to create an
 * instance of the Vizkit plugin using ruby.
 */
class VizkitQtPluginBase : public QObject
{
    Q_OBJECT
    
    public:
        VizkitQtPluginBase(QObject* parent = 0) : QObject(parent){};
    
    public slots:
        virtual VizPluginBase* createPlugin(QString const& name) = 0;
        virtual QStringList getAvailablePlugins() const = 0;
};

/**
 * Macro that adds a type-specific ruby adapter, provided by the plugin.
 * Use this if you want to provide ruby adapters:
 *
 * <code>
 * Pluginname::Pluginname()
 * {    
 *     //there will be a updateWaypoint method in ruby
 *     VizPluginRubyAdapter(Pluginname, base::Waypoint, Waypoint)
 * 
 *     //if you want to call any other method of your plugin in ruby
 *     VizPluginRubyConfig(Pluginname, bool, enableSomething)
 * }
 */
#define VizPluginRubyAdapterCommon(pluginName, dataType, methodName, rubyMethodName)\
    class VizPluginRubyAdapter##pluginName##rubyMethodName : public VizPluginRubyAdapterBase {\
        public:\
            VizPluginRubyAdapter##pluginName##rubyMethodName(pluginName* plugin)\
            {\
                vizPlugin = plugin;\
            };\
            void update(QVariant& data, bool pass_ownership)\
            {\
                void* ptr = data.value<void*>();\
                dataType* pluginData = reinterpret_cast<dataType*>(ptr);\
                vizPlugin->methodName(*pluginData);\
		if (pass_ownership) \
			delete pluginData; \
            }\
        public slots:\
            QString getDataType() \
            {\
                return #dataType;\
            }\
            QString getRubyMethod() \
            {\
                return #rubyMethodName; \
            }\
        private:\
            pluginName* vizPlugin;\
    };\
    adapterCollection.addAdapter(new VizPluginRubyAdapter##pluginName##rubyMethodName(this));

#define VizPluginRubyAdapter(pluginName, dataType, typeName) \
    VizPluginRubyAdapterCommon(pluginName, dataType, updateData, update##typeName)
#define VizPluginRubyMethod(pluginName, dataType, methodName) \
    VizPluginRubyAdapterCommon(pluginName, dataType, methodName, methodName)
#define VizPluginRubyConfig(pluginName, dataType, methodName) \
    VizPluginRubyAdapterCommon(pluginName, dataType, methodName, methodName)


/**
 * Macro that adds a Vizkit Qt plugin to a Vizkit plugin.
 * This is needed to create an instance of the plugin in ruby, if
 * the plugin is part of a external library.
 * The ruby adapter macro is also needed in this case.
 * 
 * Use this to provide a Vizkit Qt plugin:
 *
 * <code>
 *     class WaypointVisualization{..};
 *     
 *     VizkitQtPlugin(WaypointVisualization)
 */
#define VizkitQtPlugin(pluginName)\
    class QtPlugin##pluginName : public vizkit::VizkitQtPluginBase {\
        public:\
        virtual QStringList getAvailablePlugins() const\
        {\
            QStringList result; \
            result.push_back(#pluginName); \
            return result;\
        } \
        virtual vizkit::VizPluginBase* createPlugin(QString const& name)\
        {\
            if (name == #pluginName) \
                return new pluginName;\
            else return 0;\
        };\
    };\
    Q_EXPORT_PLUGIN2(QtPlugin##pluginName, QtPlugin##pluginName)


/** @deprecated adapter item for legacy visualizations. Do not derive from this
 * class for new designs. Use VizPlugin directly instead.
 */
template <class T>
class VizPluginAdapter : public VizPlugin<T>
{
    protected:
	virtual void operatorIntern( osg::Node* node, osg::NodeVisitor* nv ) = 0;

        VizPluginAdapter()
	    : groupNode(new osg::Group())
        {
        }

	osg::ref_ptr<osg::Node> createMainNode()
	{
	    return groupNode;
	}

	void updateMainNode( osg::Node* node )
	{
	    // NULL for nodevisitor is ok here, since its not used anywhere
	    operatorIntern( node, NULL );
	}

	void setMainNode( osg::Node* node )
	{
	    groupNode->addChild( node );
	}

    protected:
	osg::ref_ptr<osg::Group> groupNode;
	osg::ref_ptr<osg::Node> ownNode;
};

}
#endif
