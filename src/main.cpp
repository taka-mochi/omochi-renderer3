#include "stdafx.h"

#include "renderer/PathTracer.h"
#include "renderer/Camera.h"
#include "renderer/LinearGammaToonMapper.h"
#include "scenes/CornellBoxScene.h"
#include "scenes/TestScene.h"
#include "scenes/IBLTestScene.h"
#include "scenes/SceneFromExternalFile.h"
#include "scenes/SceneFromExternalFileFactory.h"
#include "tools/Vector.h"
#include "viewer/WindowViewer.h"
#include "renderer/Settings.h"
#include "tools/PPMSaver.h"
#include "tools/PNGSaver.h"
#include "tools/FileSaverCallerWithTimer.h"

#include <omp.h>

using namespace std;
using namespace OmochiRenderer;

void initSceneFactories()
{
  auto &factoryManager = SceneFactoryManager::GetInstance();

  factoryManager.Register("CornellBoxScene", std::make_shared<CornellBoxSceneFactory>());
  factoryManager.Register("IBLTestScene", std::make_shared<IBLTestSceneFactory>());
  factoryManager.Register("SceneFromExternalFile", std::make_shared<SceneFromExternalFileFactory>());
  factoryManager.Register("TestScene", std::make_shared<TestSceneFactory>());
}

int main(int argc, char *argv[]) {

  initSceneFactories();

  std::shared_ptr<Settings> settings = std::make_shared<Settings>();

  // set renderer and scene
  std::string settingfile = "settings.txt";
  if (argc >= 2) { settingfile = argv[1]; }
  if (!settings->LoadFromFile(settingfile)) {
    std::cerr << "Failed to load " << settingfile << std::endl;
    return -1;
  }

  // ファイル保存用インスタンス
  //std::shared_ptr<PPMSaver> saver = std::make_shared<PPMSaver>(settings);
  std::shared_ptr<PNGSaver> saver = std::make_shared<PNGSaver>(settings);

  PathTracer::RenderingFinishCallbackFunction callback([&saver](int samples, const Color *img, double accumulatedRenderingTime) {
      // レンダリング完了時に呼ばれるコールバックメソッド
      cerr << "save ppm file for sample " << samples << " ..." << endl;
      saver->Save(samples, img, accumulatedRenderingTime);
      cerr << "Total rendering time = " << accumulatedRenderingTime << " min." << endl;
  });

  if (!settings->DoSaveOnEachSampleEnded()) {
    callback = nullptr;
  }

  Camera camera(settings->GetWidth(), settings->GetHeight(), settings->GetCameraPosition(), settings->GetCameraDirection(),
    settings->GetCameraUp(), settings->GetScreenHeightInWorldCoordinate(), settings->GetDistanceFromCameraToScreen());

  std::shared_ptr<PathTracer> renderer = std::make_shared<PathTracer>(
    camera, settings->GetSampleStart(), settings->GetSampleEnd(), settings->GetSampleStep(), settings->GetSuperSamples(), callback);
  renderer->EnableNextEventEstimation(Utils::parseBoolean(settings->GetRawSetting("next event estimation")));

  FileSaverCallerWithTimer timeSaver(renderer, saver);
  timeSaver.SetSaveTimerInformation(settings->GetSaveSpan());
  timeSaver.StartTimer();

  // シーン生成
  auto sceneFactory = SceneFactoryManager::GetInstance().Get(settings->GetSceneType());
  if (sceneFactory == nullptr) {
    cerr << "Scene type: " << settings->GetSceneType() << " is invalid!!!" << endl;
    return -1;
  }
  std::shared_ptr<Scene> scene = sceneFactory->Create(settings->GetSceneInformation());
  //IBLTestScene scene;
  /*SceneFromExternalFile scene(settings->GetSceneFile());
  if (!scene.IsValid()) {
    cerr << "faild to load scene: " << settings->GetSceneFile() << endl;
    return -1;
  }*/
  //CornellBoxScene scene;

  omp_set_num_threads(settings->GetNumberOfThreads());

  clock_t startTime;

  // set window viewer
  LinearGammaToonMapper mapper;
  WindowViewer viewer("OmochiRenderer", camera, *renderer, mapper);
  if (settings->DoShowPreview()) {
    viewer.StartViewerOnNewThread();
    viewer.SetCallbackFunctionWhenWindowClosed(std::function<void(void)>(
      [&startTime]{
        cerr << "total time = " << (1.0 / 60 * (clock() - startTime) / CLOCKS_PER_SEC) << " (min)." << endl;
        exit(0);
      }
    ));
  }

  // start
  cerr << "begin rendering..." << endl;
  startTime = clock();
	renderer->RenderScene(*scene);
  cerr << "total time = " << (1.0 / 60 * (clock() - startTime) / CLOCKS_PER_SEC) << " (min)." << endl;

  if (settings->DoShowPreview()) {
    viewer.WaitWindowFinish();
  }
  timeSaver.StopAndWaitStopping();

  return 0;
}
