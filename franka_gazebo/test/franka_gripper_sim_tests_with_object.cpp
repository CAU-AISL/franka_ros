#include <actionlib/client/simple_action_client.h>
#include <actionlib/client/terminal_state.h>
#include <franka_gripper/GraspAction.h>
#include <franka_gripper/MoveAction.h>
#include <gtest/gtest.h>
#include <ros/ros.h>

#include "gripper_sim_test_setup.h"

static const double kAllowedPositionError = 5e-3;
static const double kAllowedForceError = 0.1;
static const double kAllowedRelativeDurationError = 0.2;
static const double kStoneWidth = 0.032;

class GripperGraspFixtureTest
    : public GripperSimTestSetup,
      public testing::WithParamInterface<std::tuple<std::tuple<double, double, double>, double>> {
 protected:
  void SetUp() override {
    GripperSimTestSetup::SetUp();
    std::tie(desired_width, desired_velocity, desired_force) = std::get<0>(GetParam());
    desired_sleep = std::get<1>(GetParam());
  }

  double desired_width{0};
  double desired_velocity{0};
  double desired_force{0};
  double desired_sleep{0};
};

class GripperFailGraspFixtureTest : public GripperGraspFixtureTest {};

TEST_F(GripperSimTestSetup, FailMove) {  // NOLINT(cert-err58-cpp)
  double desired_width = 0;
  double desired_velocity = 0.1;

  auto move_goal = franka_gripper::MoveGoal();
  move_goal.width = desired_width;
  move_goal.speed = desired_velocity;
  move_client->sendGoal(move_goal);

  bool finished_before_timeout = move_client->waitForResult(ros::Duration(15.0));
  EXPECT_TRUE(finished_before_timeout);
  EXPECT_TRUE(move_client->getState() == actionlib::SimpleClientGoalState::SUCCEEDED);
  EXPECT_FALSE(move_client->getResult()->success);

  move_goal.width = 0.08;
  move_client->sendGoal(move_goal);
  finished_before_timeout = move_client->waitForResult(ros::Duration(15.0));
  EXPECT_TRUE(finished_before_timeout);
  EXPECT_TRUE(move_client->getState() == actionlib::SimpleClientGoalState::SUCCEEDED);
  EXPECT_TRUE(move_client->getResult()->success);
}

TEST_P(GripperGraspFixtureTest, CanGrasp) {  // NOLINT(cert-err58-cpp)
  double start_width = 0.08;
  auto move_goal = franka_gripper::MoveGoal();
  move_goal.width = start_width;
  move_goal.speed = 0.1;
  this->move_client->sendGoal(move_goal);
  bool finished_before_timeout = move_client->waitForResult(ros::Duration(10.0));
  EXPECT_TRUE(finished_before_timeout);
  ros::Duration(desired_sleep).sleep();
  double expected_duration = (start_width - kStoneWidth) / desired_velocity;
  auto grasp_goal = franka_gripper::GraspGoal();
  grasp_goal.width = desired_width;
  grasp_goal.speed = desired_velocity;
  grasp_goal.force = desired_force;
  grasp_goal.epsilon.inner = 0.005;
  grasp_goal.epsilon.outer = 0.005;
  auto start_time = ros::Time::now();
  this->grasp_client->sendGoal(grasp_goal);
  finished_before_timeout = this->grasp_client->waitForResult(ros::Duration(10.0));
  auto stop_time = ros::Time::now();
  double duration = (stop_time - start_time).toSec();
  UpdateFingerState();
  EXPECT_TRUE(finished_before_timeout);
  EXPECT_NEAR(finger_1_pos * 2, kStoneWidth, kAllowedPositionError);
  EXPECT_NEAR(finger_2_pos * 2, kStoneWidth, kAllowedPositionError);
  EXPECT_NEAR(duration, expected_duration, expected_duration * kAllowedRelativeDurationError);
  double expected_force = desired_force / 2.0;
  EXPECT_NEAR(finger_1_force, expected_force, kAllowedForceError);
  EXPECT_NEAR(finger_2_force, expected_force, kAllowedForceError);
  EXPECT_TRUE(grasp_client->getState() == actionlib::SimpleClientGoalState::SUCCEEDED);
  EXPECT_TRUE(grasp_client->getResult()->success);
}

TEST_P(GripperFailGraspFixtureTest, CanFailGrasp) {  // NOLINT(cert-err58-cpp)
  double start_width = 0.08;
  auto move_goal = franka_gripper::MoveGoal();
  move_goal.width = start_width;
  move_goal.speed = 0.1;
  this->move_client->sendGoal(move_goal);
  bool finished_before_timeout = move_client->waitForResult(ros::Duration(10.0));
  EXPECT_TRUE(finished_before_timeout);
  ros::Duration(desired_sleep).sleep();
  double expected_duration = (start_width - kStoneWidth) / desired_velocity;
  auto grasp_goal = franka_gripper::GraspGoal();
  grasp_goal.width = desired_width;
  grasp_goal.speed = desired_velocity;
  grasp_goal.force = desired_force;
  grasp_goal.epsilon.inner = 0.005;
  grasp_goal.epsilon.outer = 0.005;
  auto start_time = ros::Time::now();
  this->grasp_client->sendGoal(grasp_goal);
  finished_before_timeout = this->grasp_client->waitForResult(ros::Duration(10.0));
  auto stop_time = ros::Time::now();
  double duration = (stop_time - start_time).toSec();
  UpdateFingerState();
  EXPECT_TRUE(finished_before_timeout);
  EXPECT_NEAR(finger_1_pos * 2, kStoneWidth, kAllowedPositionError);
  EXPECT_NEAR(finger_2_pos * 2, kStoneWidth, kAllowedPositionError);
  EXPECT_NEAR(duration, expected_duration, expected_duration * kAllowedRelativeDurationError);
  EXPECT_TRUE(grasp_client->getState() == actionlib::SimpleClientGoalState::SUCCEEDED);
  EXPECT_FALSE(grasp_client->getResult()->success);
}

INSTANTIATE_TEST_CASE_P(GripperFailGraspFixtureTest,  // NOLINT(cert-err58-cpp)
                        GripperFailGraspFixtureTest,
                        ::testing::Combine(::testing::Values(std::make_tuple(0.04, 0.1, 0.),
                                                             std::make_tuple(0.02, 0.1, 2.)),
                                           ::testing::Values(0, 0.1)));

INSTANTIATE_TEST_CASE_P(GripperGraspFixtureTest,  // NOLINT(cert-err58-cpp)
                        GripperGraspFixtureTest,
                        ::testing::Combine(::testing::Values(std::make_tuple(0.032, 0.1, 0.),
                                                             std::make_tuple(0.03, 0.1, 5.),
                                                             std::make_tuple(0.03, 0.01, 0.),
                                                             std::make_tuple(0.034, 0.01, 5.)),
                                           ::testing::Values(0, 0.1)));

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  ros::init(argc, argv, "franka_gripper_sim_test");
  return RUN_ALL_TESTS();
}
