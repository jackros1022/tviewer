#include <random>
#include "qt_example.h"
#include "ui_qt_example.h"

#include <pcl/point_types.h>
#include <pcl/point_cloud.h>
#include <pcl/sample_consensus/ransac.h>
#include <pcl/sample_consensus/sac_model_line.h>
#include <pcl/sample_consensus/sac_model_plane.h>
#include <pcl/sample_consensus/sac_model_sphere.h>

#include "tviewer/tviewer.h"

using namespace tviewer;

QtExample::QtExample (QWidget* parent)
: QMainWindow (parent)
, ui_ (new Ui::QtExample)
, dataset_ (new pcl::PointCloud<pcl::PointXYZ>)
{
  ui_->setupUi (this);

  connect (ui_->viewer,
           SIGNAL (pointPicked (QString, int)),
           this,
           SLOT (onPointPicked (QString, int)));

  generateDataset ();

  ui_->viewer->add
  ( CreatePointCloudObject<pcl::PointXYZ> ("dataset", "d")
  . description                           ("Generated dataset")
  . data                                  (dataset_)
  . pointSize                             (2)
  . visibility                            (1.0)
  . color                                 (generateRandomColor ())
  , true
  );

  ui_->viewer->add
  ( CreatePointCloudObject<pcl::PointXYZ> ("inliers", "c")
  . description                           ("Model inliers")
  . onUpdate                              ([this]()
    {
      pcl::PointCloud<pcl::PointXYZ>::Ptr inliers;
      inliers.reset (new pcl::PointCloud<pcl::PointXYZ>);
      pcl::copyPointCloud (*dataset_, inliers_, *inliers);
      return inliers;
    })
  . pointSize                             (5)
  . visibility                            (1.0)
  . color                                 (generateRandomColor ())
  );

  ui_->menu_bar->addMenu (ui_->viewer->getMenu ("View"));
}

QtExample::~QtExample ()
{
  delete ui_;
}

void
QtExample::onDataGenerationSettingsChanged ()
{
  generateDataset ();
  inliers_.clear ();
  ui_->viewer->update ("dataset");
  ui_->viewer->update ("inliers");
}

void
QtExample::onButtonFitClicked ()
{
  pcl::SampleConsensusModel<pcl::PointXYZ>::Ptr model;

  if (ui_->radio_line->isChecked ())
  {
    model.reset (new pcl::SampleConsensusModelLine<pcl::PointXYZ> (dataset_));
  }
  else if (ui_->radio_plane->isChecked ())
  {
    model.reset (new pcl::SampleConsensusModelPlane<pcl::PointXYZ> (dataset_));
  }
  else if (ui_->radio_sphere->isChecked ())
  {
    model.reset (new pcl::SampleConsensusModelSphere<pcl::PointXYZ> (dataset_));
  }

  pcl::RandomSampleConsensus<pcl::PointXYZ> ransac (model);
  ransac.setDistanceThreshold (ui_->spinbox_distance_threshold->value ());
  ransac.setProbability (ui_->spinbox_probability->value ());
  ransac.setMaxIterations (ui_->spinbox_max_iterations->value ());
  ransac.computeModel (1);
  ransac.getInliers (inliers_);
  Eigen::VectorXf model_coefficients;
  ransac.getModelCoefficients (model_coefficients);
  std::cout << model_coefficients << std::endl;
  //pcl::PointCloud<pcl::PointXYZ>::Ptr out (new pcl::PointCloud<pcl::PointXYZ>);
  //pcl::copyPointCloud<pcl::PointXYZ>(*dataset_, inliers, *inliers_);
  //auto n_inliers_count = model->countWithinDistance (model_coefficients, ui_->spinbox_distance_threshold->value ());
  //std::cout << "Inliers: " << n_inliers_count << " out of " << dataset_->size () << std::endl;
  std::cout << "Distances:" << std::endl;
  for (size_t i = 0; i < inliers_.size (); ++i)
  {
    //auto d = (dataset_->at (inliers_.at (i)).getVector3fMap () - model_coefficients.head<3> ()).cast<double> ();
    double dx = dataset_->at (inliers_.at (i)).x - model_coefficients[0];
    double dy = dataset_->at (inliers_.at (i)).y - model_coefficients[1];
    double dz = dataset_->at (inliers_.at (i)).z - model_coefficients[2];
    double dist = std::abs (  (dx * dx + dy * dy + dz * dz) - model_coefficients[3] * model_coefficients[3]);
    //float dist = std::abs (d.norm () - model_coefficients[3]);
    std::cout << dist << std::endl;
  }
  ui_->viewer->update ("inliers");
  ui_->viewer->show ("inliers");
}

void
QtExample::onPointPicked (const QString& name, int index)
{
  std::cout << name.toStdString() << std::endl;
}

void
QtExample::generateDataset ()
{
  static std::random_device rnd;
  static std::mt19937 gen (rnd ());

  float size = 100.0;

  pcl::PointXYZ center (0, 0, 0);
  auto push_point = [&](const Eigen::Vector3f& p)
  {
    dataset_->push_back (center);
    dataset_->back ().getVector3fMap () += p;
  };

  std::uniform_real_distribution<> o_dist (-size / 2, size / 2);
  auto generate_outlier = [&](){ return Eigen::Vector3f (o_dist (gen), o_dist (gen), o_dist (gen)); };

  size_t N = ui_->spinbox_ppm->value ();
  dataset_->clear ();

  if (ui_->checkbox_line->checkState ())
  {
    // Generate model parameters
    std::uniform_real_distribution<> m_dist (0.0, M_PI / 2);
    float theta = m_dist (gen);
    float phi = m_dist (gen);
    // Create generator
    std::uniform_real_distribution<> p_dist (-size / 2, size / 2);
    Eigen::Vector3f v (std::sin (theta) * std::cos (phi), std::sin (theta) * std::sin (phi), std::cos (theta));
    auto generate_inlier = [&](){ return v * p_dist (gen); };
    // Generate inliers
    size_t inliers = ui_->spinbox_inlier_fraction->value () * N;
    for (size_t i = 0; i < inliers; ++i)
      push_point (generate_inlier ());
    // Generate outliers
    for (size_t i = 0; i < N - inliers; ++i)
      push_point (generate_outlier ());
    // Update center
    center.x += size;
  }

  if (ui_->checkbox_sphere->checkState ())
  {
    // Generate model parameters
    std::uniform_real_distribution<> m_dist (size / 5, size / 2);
    float r = m_dist (gen);
    // Create generator
    std::uniform_real_distribution<> p_dist (-1.0, 1.0);
    auto generate_inlier = [&]()
    {
      float x = 1.0, y = 1.0;
      while (x * x + y * y >= 1.0)
      {
        x = p_dist (gen);
        y = p_dist (gen);
      }
      float s = 2 * std::sqrt (1 - x * x - y * y) * r;
      return Eigen::Vector3f (s * x, s * y, (1.0 - 2.0 * (x * x + y * y)) * r);
    };
    // Generate inliers
    size_t inliers = ui_->spinbox_inlier_fraction->value () * N;
    for (size_t i = 0; i < inliers; ++i)
      push_point (generate_inlier ());
    // Generate outliers
    for (size_t i = 0; i < N - inliers; ++i)
      push_point (generate_outlier ());
    // Update center
    center.x += size;
  }
}
