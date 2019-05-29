/**
 * particle_filter.cpp
 *
 * Created on: Dec 12, 2016
 * Author: Tiffany Huang
 */

#include "particle_filter.h"

#include <math.h>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "helper_functions.h"

using std::string;
using std::vector;

void ParticleFilter::init(double x, double y, double theta, double std[]) {
  /**
   * TODO: Set the number of particles. Initialize all particles to 
   *   first position (based on estimates of x, y, theta and their uncertainties
   *   from GPS) and all weights to 1. 
   * TODO: Add random Gaussian noise to each particle.
   * NOTE: Consult particle_filter.h for more information about this method 
   *   (and others in this file).
   */
  num_particles = 100;  // TODO: Set the number of particles

  // define normal distributions for sensor noise
  std::random_device rd{};
  std::mt19937 gen{rd()};
  // values near the mean are the most likely
  // standard deviation affects the dispersion of generated values from the mean
  std::normal_distribution<double> N_x_init(0, std[0]);
  std::normal_distribution<double> N_y_init(0, std[1]);
  std::normal_distribution<double> N_theta_init(0, std[2]);

  // init particles
  for (int i = 0; i < num_particles; i++) {
    Particle particle;

    particle.x = N_x_init(gen);
    particle.y = N_y_init(gen);
    particle.theta = N_theta_init(gen);
    particle.weight = 1.0;

    particles.push_back(particle);
  }

  is_initialized = true;

}

void ParticleFilter::prediction(double delta_t, double std_pos[], 
                                double velocity, double yaw_rate) {
  /**
   * TODO: Add measurements to each particle and add random Gaussian noise.
   * NOTE: When adding noise you may find std::normal_distribution 
   *   and std::default_random_engine useful.
   *  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
   *  http://www.cplusplus.com/reference/random/default_random_engine/
   */
  // define normal distributions for sensor noise
  std::random_device rd{};
  std::mt19937 gen{rd()};
  // values near the mean are the most likely
  // standard deviation affects the dispersion of generated values from the mean
  std::normal_distribution<double> N_x(0, std_pos[0]);
  std::normal_distribution<double> N_y(0, std_pos[1]);
  std::normal_distribution<double> N_theta(0, std_pos[2]);

  for (int i = 0; i < num_particles; i++) {

    // calculate new state
    particles[i].x += velocity/yaw_rate*(sin(particles[i].theta + yaw_rate*delta_t) - sin(particles[i].theta));
    particles[i].y += velocity/yaw_rate*(cos(particles[i].theta) - cos(particles[i].theta + yaw_rate*delta_t));
    particles[i].theta += yaw_rate*delta_t;

    // add noise
    particles[i].x += N_x(gen);
    particles[i].y += N_y(gen);
    particles[i].theta += N_theta(gen);
  }

}

void ParticleFilter::dataAssociation(vector<LandmarkObs> predicted, 
                                     vector<LandmarkObs>& observations) {
  /**
   * TODO: Find the predicted measurement that is closest to each 
   *   observed measurement and assign the observed measurement to this 
   *   particular landmark.
   * NOTE: this method will NOT be called by the grading code. But you will 
   *   probably find it useful to implement this method and use it as a helper 
   *   during the updateWeights phase.
   */
  for (int i = 0; i < observations.size(); i++) {
    
    // grab current observation
    LandmarkObs o = observations[i];

    // init minimum distance to maximum possible
    double min_dist = std::numeric_limits<double>::max();

    // init id of landmark from map placeholder to be associated with the observation
    int map_id = -1;
    
    for (int j = 0; j < predicted.size(); j++) {
      // grab current prediction
      LandmarkObs p = predicted[j];
      
      // get distance between current/predicted landmarks
      double cur_dist = dist(o.x, o.y, p.x, p.y);

      // find the predicted landmark nearest the current observed landmark
      if (cur_dist < min_dist) {
        min_dist = cur_dist;
        map_id = p.id;
      }
    }

    // set the observation's id to the nearest predicted landmark's id
    observations[i].id = map_id;
  }
   

}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[],
                                   const vector<LandmarkObs> &observations, 
                                   const Map &map_landmarks) {
  /**
   * TODO: Update the weights of each particle using a mult-variate Gaussian 
   *   distribution. You can read more about this distribution here: 
   *   https://en.wikipedia.org/wiki/Multivariate_normal_distribution
   * NOTE: The observations are given in the VEHICLE'S coordinate system. 
   *   Your particles are located according to the MAP'S coordinate system. 
   *   You will need to transform between the two systems. Keep in mind that
   *   this transformation requires both rotation AND translation (but no scaling).
   *   The following is a good resource for the theory:
   *   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
   *   and the following is a good resource for the actual equation to implement
   *   (look at equation 3.33) http://planning.cs.uiuc.edu/node99.html
   */

  // for each particle...
  for (int i = 0; i < num_particles; i++) {

    // get the particle x, y coordinates
    Particle particle = particles[i];

    // create a vector to hold the map landmark locations predicted to be within sensor range of the particle
    vector<LandmarkObs> predictions;

    // for each map landmark...
    for (int j = 0; j < map_landmarks.landmark_list.size(); j++) {

      // get id and x,y coordinates
      Map::single_landmark_s lm = map_landmarks.landmark_list[j];
      // only consider landmarks within sensor range of the particle (rather than using the "dist" method considering a circular 
      // region around the particle, this considers a rectangular region but is computationally faster)
      if (fabs(lm.x_f - particle.x) <= sensor_range && fabs(lm.y_f - particle.y) <= sensor_range) {
        // add prediction to vector
        LandmarkObs prediction;
        prediction.x = lm.x_f;
        prediction.y = lm.y_f;
        prediction.id = lm.id_i;

        predictions.push_back(prediction);
      }
    }

    // create and populate a copy of the list of observations transformed from vehicle coordinates to map coordinates
    vector<LandmarkObs> t_observations;
    for (int j = 0; j < observations.size(); j++) {
      LandmarkObs t_o;
      t_o.x = cos(particle.theta)*observations[j].x - sin(particle.theta)*observations[j].y + particle.x;
      t_o.y = sin(particle.theta)*observations[j].x + cos(particle.theta)*observations[j].y + particle.y;
      t_o.id = observations[j].id;
      t_observations.push_back(t_o);
    }

    // perform dataAssociation for the predictions and transformed observations on current particle
    dataAssociation(predictions, t_observations);

    // reinit weight
    particles[i].weight = 1.0;

    for (int j = 0; j < t_observations.size(); j++) {
      
      // placeholders for observation and associated prediction coordinates
      LandmarkObs t_o, p;
      t_o = t_observations[j];

      int associated_prediction = t_observations[j].id;

      // get the x,y coordinates of the prediction associated with the current observation
      for (int k = 0; k < predictions.size(); k++) {
        if (predictions[k].id == t_o.id) {
          p = predictions[k];
        }
      }

      // calculate weight for this observation with multivariate Gaussian
      double sig_x, sig_y, x_obs, y_obs, mu_x, mu_y;
      sig_x = std_landmark[0];
      sig_y = std_landmark[1];
      x_obs = p.x;
      y_obs = p.y;
      mu_x = t_o.x;
      mu_y = t_o.y;
      // calculate normalization term
      double gauss_norm;
      gauss_norm = 1/(2*M_PI*sig_x*sig_y);
      // calculate exponent
      double exponent;
      exponent = (pow(x_obs - mu_x, 2)/(2*pow(sig_x, 2))) + (pow(y_obs - mu_y, 2)/(2*pow(sig_y, 2)));
      // calculate weight using normalization terms and exponent
      double weight;
      weight = gauss_norm*exp(-exponent);

      // product of this obersvation weight with total observations weight
      particles[i].weight *= weight;
    }
  }
 
}

void ParticleFilter::resample() {
  /**
   * TODO: Resample particles with replacement with probability proportional 
   *   to their weight. 
   * NOTE: You may find std::discrete_distribution helpful here.
   *   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution
   */

  vector<Particle> new_particles;

  // get all of the current weights
  vector<double> weights;
  for (int i = 0; i < num_particles; i++) {
    weights.push_back(particles[i].weight);
  }

  // generate random starting index for resampling wheel
  std::random_device rd{};
  std::mt19937 gen{rd()};
  // values near the mean are the most likely
  // standard deviation affects the dispersion of generated values from the mean
  std::uniform_int_distribution<int> uniintdist(0, num_particles-1);
  auto index = uniintdist(gen);

  // get max weight
  auto maxPosition = std::max_element(weights.begin(), weights.end());
  double max_weight = *maxPosition;

  // uniform random distribution [0.0, max_weight)
  std::uniform_real_distribution<double> unirealdist(0.0, max_weight);

  double beta = 0.0;

  // spin the resample wheel!
  for (int i = 0; i < num_particles; i++) {
    beta += unirealdist(gen) * 2.0;
    while (beta > weights[index]) {
      beta -= weights[index];
      index = (index + 1)%num_particles;
    }
    new_particles.push_back(particles[index]);
  }

  particles = new_particles;
}

void ParticleFilter::SetAssociations(Particle& particle, 
                                     const vector<int>& associations, 
                                     const vector<double>& sense_x, 
                                     const vector<double>& sense_y) {
  // particle: the particle to which assign each listed association, 
  //   and association's (x,y) world coordinates mapping
  // associations: The landmark id that goes along with each listed association
  // sense_x: the associations x mapping already converted to world coordinates
  // sense_y: the associations y mapping already converted to world coordinates
  particle.associations= associations;
  particle.sense_x = sense_x;
  particle.sense_y = sense_y;
}

string ParticleFilter::getAssociations(Particle best) {
  vector<int> v = best.associations;
  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<int>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;
}

string ParticleFilter::getSenseCoord(Particle best, string coord) {
  vector<double> v;

  if (coord == "X") {
    v = best.sense_x;
  } else {
    v = best.sense_y;
  }

  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<float>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;
}