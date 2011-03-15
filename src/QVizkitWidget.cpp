#include "QVizkitWidget.hpp"
#include <QVBoxLayout>
#include <GridNode.hpp>
#include <vizkit/MotionCommandVisualization.hpp>
#include <vizkit/TrajectoryVisualization.hpp>
#include <vizkit/WaypointVisualization.hpp>

using namespace vizkit;

QVizkitWidget::QVizkitWidget( QWidget* parent, Qt::WindowFlags f )
    : CompositeViewerQOSG( parent, f )
{
    createSceneGraph();

    QWidget* viewWidget = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout;
    layout->addWidget( viewWidget );
    this->setLayout( layout );

    view = new ViewQOSG( viewWidget );
    view->setSizePolicy( QSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding ) );
    view->setData( root );
    addView( view );

    // pickhandler is for selecting objects in the opengl view
    pickHandler = new PickHandler();
    view->addEventHandler( pickHandler );
    
    // add visualization of ground grid 
    GridNode *gn = new GridNode();
    root->addChild(gn);
    
    pluginNames = new QStringList();
}

QSize QVizkitWidget::sizeHint() const
{
    return QSize( 800, 600 );
}

osg::ref_ptr<osg::Group> QVizkitWidget::getRootNode() const
{
    return root;
}

void QVizkitWidget::setTrackedNode( VizPluginBase* plugin )
{
    view->setTrackedNode(plugin->getVizNode());
}

void QVizkitWidget::createSceneGraph() 
{
    //create root node that holds all other nodes
    root = new osg::Group;
    
    osg::ref_ptr<osg::StateSet> state = root->getOrCreateStateSet();
    state->setGlobalDefaults();
    state->setMode( GL_LINE_SMOOTH, osg::StateAttribute::ON );
    state->setMode( GL_POINT_SMOOTH, osg::StateAttribute::ON );
    state->setMode( GL_BLEND, osg::StateAttribute::ON );    
    state->setMode( GL_DEPTH_TEST, osg::StateAttribute::ON);
    state->setMode( GL_LIGHTING, osg::StateAttribute::ON );
    state->setMode( GL_LIGHT0, osg::StateAttribute::ON );
    state->setMode( GL_LIGHT1, osg::StateAttribute::ON );
	
    root->setDataVariance(osg::Object::DYNAMIC);

    // Add the Light to a LightSource. Add the LightSource and
    //   MatrixTransform to the scene graph.
    for(size_t i=0;i<2;i++)
    {
	osg::ref_ptr<osg::Light> light = new osg::Light;
	light->setLightNum(i);
	switch(i) {
	    case 0:
		light->setAmbient( osg::Vec4( .1f, .1f, .1f, 1.f ));
		light->setDiffuse( osg::Vec4( .8f, .8f, .8f, 1.f ));
		light->setSpecular( osg::Vec4( .8f, .8f, .8f, 1.f ));
		light->setPosition( osg::Vec4( 1.f, 1.5f, 2.f, 0.f ));
		break;
	    case 1:
		light->setAmbient( osg::Vec4( .1f, .1f, .1f, 1.f ));
		light->setDiffuse( osg::Vec4( .1f, .3f, .1f, 1.f ));
		light->setSpecular( osg::Vec4( .1f, .3f, .1f, 1.f ));
		light->setPosition( osg::Vec4( -1.f, -3.f, 1.f, 0.f ));
	}

	osg::ref_ptr<osg::LightSource> ls = new osg::LightSource;
	ls->setLight( light.get() );
	//ls->setStateSetModes(*state, osg::StateAttribute::ON);
	root->addChild( ls.get() );
    }
}

void QVizkitWidget::addDataHandler(VizPluginBase *viz)
{
    root->addChild( viz->getVizNode() );
}

void QVizkitWidget::removeDataHandler(VizPluginBase *viz)
{
    root->removeChild( viz->getVizNode() );
}

/**
 * Sets the camera focus to specific position.
 * @param lookAtPos focus this point
 */
void QVizkitWidget::changeCameraView(const osg::Vec3& lookAtPos)
{
    osgGA::KeySwitchMatrixManipulator* switchMatrixManipulator = dynamic_cast<osgGA::KeySwitchMatrixManipulator*>(view->getCameraManipulator());
    if (!switchMatrixManipulator) return;
    //select TerrainManipulator
    switchMatrixManipulator->selectMatrixManipulator(3);
    
    //get current eye position
    osg::Matrixd matrix = switchMatrixManipulator->getMatrix();
    osg::Vec3d currentEyePos = matrix.getTrans();
    
    changeCameraView(lookAtPos, currentEyePos);
}

/**
 * Sets the camera focus and the camera itself to specific position.
 * @param lookAtPos focus this point
 * @param eyePos position of the camera
 */
void QVizkitWidget::changeCameraView(const osg::Vec3& lookAtPos, const osg::Vec3& eyePos)
{
    osgGA::KeySwitchMatrixManipulator* switchMatrixManipulator = dynamic_cast<osgGA::KeySwitchMatrixManipulator*>(view->getCameraManipulator());
    if (!switchMatrixManipulator) return;
    //select TerrainManipulator
    switchMatrixManipulator->selectMatrixManipulator(3);
    
    //get last values of eye, center and up
    osg::Vec3d eye, center, up;
    switchMatrixManipulator->getHomePosition(eye, center, up);

    //set new values
    switchMatrixManipulator->setHomePosition(eyePos, lookAtPos, up);

    view->home();
}

/**
 * Creates an instance of a visualization plugin given by its name 
 * and returns the adapter collection of the plugin, used in ruby.
 * @param pluginName Name of the plugin
 * @return Instanc of the adapter collection of this plugin
 */
QObject* vizkit::QVizkitWidget::createPlugin(QString pluginName)
{
    vizkit::VizPluginBase* plugin = 0;
    if (pluginName == "WaypointVisualization")
    {
        plugin = new vizkit::WaypointVisualization();
    }
    else if (pluginName == "MotionCommandVisualization")
    {
        plugin = new vizkit::MotionCommandVisualization();
    }
    else if (pluginName == "TrajectoryVisualization")
    {
        plugin = new vizkit::TrajectoryVisualization();
    }

    if (plugin) 
    {
        this->addDataHandler(plugin);
        VizPluginRubyAdapterCollection* adapterCollection = plugin->getRubyAdapterCollection();
        return adapterCollection;
    }
    else {
        std::cerr << "The Pluginname " << pluginName.toStdString() << " is unknown!" << std::endl;
        return NULL;
    }
}

/**
 * Returns a list of all available visualization plugins.
 * @return list of plugin names
 */
QStringList* vizkit::QVizkitWidget::getListOfAvailablePlugins()
{
    if (!pluginNames->size()) 
    {
        pluginNames->push_back("WaypointVisualization");
        pluginNames->push_back("TrajectoryVisualization");
        pluginNames->push_back("MotionCommandVisualization");
    }
    return pluginNames;
}

void vizkit::QVizkitWidget::addPlugin(QObject* plugin)
{
    vizkit::VizPluginWidgetBase* pluginWidget = dynamic_cast<vizkit::VizPluginWidgetBase*>(plugin);
    if 
        (pluginWidget) addDataHandler(pluginWidget->getPlugin());
    else
        std::cerr << "The given attribute is no VizPlugin!" << std::endl;
}

void vizkit::QVizkitWidget::removePlugin(QObject* plugin)
{
    vizkit::VizPluginWidgetBase* pluginWidget = dynamic_cast<vizkit::VizPluginWidgetBase*>(plugin);
    if 
        (pluginWidget) removeDataHandler(pluginWidget->getPlugin());
    else
        std::cerr << "The given attribute is no VizPlugin!" << std::endl;
}