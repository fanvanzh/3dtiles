#include <osg/ArgumentParser>
#include <osg/ApplicationUsage>
#include <osg/Group>
#include <osg/Notify>
#include <osg/Vec3>
#include <osg/ProxyNode>
#include <osg/Geometry>
#include <osg/Texture>
#include <osg/Texture2D>
#include <osg/Texture3D>
#include <osg/BlendFunc>
#include <osg/Timer>

#include <osg/GL>
#include <osg/GLU>

#include <osgDB/Registry>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osgDB/FileNameUtils>
#include <osgDB/ReaderWriter>
#include <osgDB/PluginQuery>

#include <osgUtil/Optimizer>
#include <osgUtil/Simplifier>
#include <osgUtil/SmoothingVisitor>

#include <osgViewer/GraphicsWindow>
#include <osgViewer/Version>


class MyGraphicsContext {
public:
	MyGraphicsContext()
	{
		osg::ref_ptr<osg::GraphicsContext::Traits> traits = new osg::GraphicsContext::Traits;
		traits->x = 0;
		traits->y = 0;
		traits->width = 1;
		traits->height = 1;
		traits->windowDecoration = false;
		traits->doubleBuffer = false;
		traits->sharedContext = 0;
		traits->pbuffer = true;

		_gc = osg::GraphicsContext::createGraphicsContext(traits.get());

		if (!_gc)
		{
			osg::notify(osg::NOTICE) << "Failed to create pbuffer, failing back to normal graphics window." << std::endl;

			traits->pbuffer = false;
			_gc = osg::GraphicsContext::createGraphicsContext(traits.get());
		}

		if (_gc.valid())
		{
			_gc->realize();
			_gc->makeCurrent();
			if (dynamic_cast<osgViewer::GraphicsWindow*>(_gc.get()))
			{
				std::cout << "Realized graphics window for OpenGL operations." << std::endl;
			}
			else
			{
				std::cout << "Realized pbuffer for OpenGL operations." << std::endl;
			}
		}
	}

	bool valid() const { return _gc.valid() && _gc->isRealized(); }

private:
	osg::ref_ptr<osg::GraphicsContext> _gc;
};

extern "C" void OSGVIEWER_EXPORT graphicswindow_Win32(void);

bool decompress(osg::Texture2D* texture2D) {
	graphicswindow_Win32();

	MyGraphicsContext context;
	if (!context.valid())
	{
		osg::notify(osg::NOTICE) << "Error: Unable to create graphis context, problem with running osgViewer cannot run compression." << std::endl;
		return false;
	}

	osg::ref_ptr<osg::State> state = new osg::State;

	//osg::Texture* texture2D = new osg::Texture2D();
	//texture2D->setImage(0, img);
	osg::ref_ptr<osg::Image> image = texture2D->getImage();
	{
		texture2D->setInternalFormatMode(osg::Texture::USE_IMAGE_DATA_FORMAT);
		// need to disable the unref after apply, otherwise the image could go out of scope.
		bool unrefImageDataAfterApply = texture2D->getUnRefImageDataAfterApply();
		texture2D->setUnRefImageDataAfterApply(false);
		// get OpenGL driver to create texture from image.
		texture2D->apply(*state);
		// restore the original setting
		texture2D->setUnRefImageDataAfterApply(unrefImageDataAfterApply);
		image->readImageFromCurrentTexture(0, true);
		texture2D->setInternalFormatMode(osg::Texture::USE_IMAGE_DATA_FORMAT);
	}
	return true;
}