/*
Copyright (c) by respective owners including Yahoo!, Microsoft, and
individual contributors. All rights reserved.  Released under a BSD (revised)
license as described in the file LICENSE.
 */
#include <stdio.h>
#include <float.h>
#include <iostream>
#include <sstream>
#include <math.h>
#include <assert.h>

#include "global_data.h"
#include "simple_label.h"
#include "parser.h"
#include "gd.h"

using namespace std;

struct global_prediction {
  float p;
  float weight;
};

size_t really_read(int sock, void* in, size_t count)
{
  char* buf = (char*)in;
  size_t done = 0;
  int r = 0;
  while (done < count)
    {
      if ((r = read(sock,buf,(unsigned int)(count-done))) == 0)
	return 0;
      else
	if (r < 0)
	  {
	    cerr << "argh! bad read! on message from " << sock << endl;
	    perror(NULL);
	    exit(0);
	  }
	else
	  {
	    done += r;
	    buf += r;
	  }
    }
  return done;
}

void get_prediction(int sock, float& res, float& weight)
{
  global_prediction p;
  size_t count = really_read(sock, &p, sizeof(p));
  res = p.p;
  weight = p.weight;
  
  assert(count == sizeof(p));
}

void send_prediction(int sock, global_prediction p)
{
  if (write(sock, &p, sizeof(p)) < (int)sizeof(p))
    {
      cerr << "argh! bad global write! " << sock << endl;
      perror(NULL);
      exit(0);
    }
}

void binary_print_result(int f, float res, float weight, v_array<char> tag)
{
  if (f >= 0)
    {
      global_prediction ps = {res, weight};
      send_prediction(f, ps);
    }
}

void print_tag(std::stringstream& ss, v_array<char> tag)
{
  if (tag.begin != tag.end){
    ss << ' ';
    ss.write(tag.begin, sizeof(char)*tag.size());
  }  
}

void print_result(int f, float res, float weight, v_array<char> tag)
{
  if (f >= 0)
    {
      char temp[30];
      sprintf(temp, "%f", res);
      std::stringstream ss;
      ss << temp;
      print_tag(ss, tag);
      ss << '\n';
      ssize_t len = ss.str().size();
      ssize_t t = write(f, ss.str().c_str(), (unsigned int)len);
      if (t != len)
        {
          cerr << "write error" << endl;
        }
    }
}

void print_raw_text(int f, string s, v_array<char> tag)
{
  if (f < 0)
    return;

  std::stringstream ss;
  ss << s;
  print_tag (ss, tag);
  ss << '\n';
  ssize_t len = ss.str().size();
  ssize_t t = write(f, ss.str().c_str(), (unsigned int)len);
  if (t != len)
    {
      cerr << "write error" << endl;
    }
}

void active_print_result(int f, float res, float weight, v_array<char> tag)
{
  if (f >= 0)
    {
      std::stringstream ss;
      char temp[30];
      sprintf(temp, "%f", res);
      ss << temp;
      print_tag(ss, tag);
      if(weight >= 0)
	{
	  sprintf(temp, " %f", weight);
          ss << temp;
	}
      ss << '\n';
      ssize_t len = ss.str().size();
      ssize_t t = write(f, ss.str().c_str(), (unsigned int)len);
      if (t != len)
	cerr << "write error" << endl;
    }
}

void print_lda_result(vw& all, int f, float* res, float weight, v_array<char> tag)
{
  if (f >= 0)
    {
      std::stringstream ss;
      char temp[30];
      for (size_t k = 0; k < all.lda; k++)
	{
	  sprintf(temp, "%f ", res[k]);
          ss << temp;
	}
      print_tag(ss, tag);
      ss << '\n';
      ssize_t len = ss.str().size();
      ssize_t t = write(f, ss.str().c_str(), (unsigned int)len);
      if (t != len)
	cerr << "write error" << endl;
    }
}

void set_mm(shared_data* sd, float label)
{
  sd->min_label = min(sd->min_label, label);
  if (label != FLT_MAX)
    sd->max_label = max(sd->max_label, label);
}

void noop_mm(shared_data* sd, float label)
{}

void vw::learn(void* a, example* ec)
{
  vw* all = (vw*)a;
  all->l.learn(a,all->l.data,ec);
}

vw::vw()
{
  sd = (shared_data *) calloc(1, sizeof(shared_data));
  sd->dump_interval = (float)exp(1.);
  sd->contraction = 1.;
  sd->max_label = 1.;
  
  p = new_parser();
  p->lp = (label_parser*)malloc(sizeof(label_parser));
  *(p->lp) = simple_label;

  reg_mode = 0;

  current_pass = 0;

  bfgs = false;
  hessian_on = false;
  stride = 1;
  num_bits = 18;
  default_bits = true;
  daemon = false;
  num_children = 10;
  lda_alpha = 0.1f;
  lda_rho = 0.1f;
  lda_D = 10000.;
  minibatch = 1;
  span_server = "";
  m = 15; 
  save_resume = false;

  l = GD::get_learner();
  scorer = l;

  set_minmax = set_mm;

  base_learner_nb_w = 1;

  power_t = 0.5;
  eta = 0.5; //default learning rate for normalized adaptive updates, this is switched to 10 by default for the other updates (see parse_args.cc)
  numpasses = 1;
  rel_threshold = 0.001f;
  rank = 0;

  final_prediction_sink.begin = final_prediction_sink.end=final_prediction_sink.end_array = NULL;
  raw_prediction = -1;
  print = print_result;
  print_text = print_raw_text;
  lda = 0;
  random_weights = false;
  per_feature_regularizer_input = "";
  per_feature_regularizer_output = "";
  per_feature_regularizer_text = "";

  options_from_file = "";

  #ifdef _WIN32
  stdout_fileno = _fileno(stdout);
  #else
  stdout_fileno = fileno(stdout);
  #endif

  searn = false;

  nonormalize = false;
  l1_lambda = 0.0;
  l2_lambda = 0.0;

  eta_decay_rate = 1.0;
  initial_weight = 0.0;

  unique_id = 0;
  total = 1;
  node = 0;

  ngram = 0;
  skips = 0;

  //by default use invariant normalized adaptive updates
  adaptive = true;
  normalized_updates = true;
  invariant_updates = true;

  normalized_sum_norm_x = 0.;
  normalized_idx = 2;

  add_constant = true;
  audit = false;
  active = false;
  active_c0 = 8.;
  active_simulation = false;
  reg.weight_vector = NULL;
  pass_length = (size_t)-1;
  passes_complete = 0;

  save_per_pass = false;

  stdin_off = false;
  do_reset_source = false;
}
