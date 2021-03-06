/*
   Copyright (C) 2013 - 2014 by Felix Bauer
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file
 * Recruitment Engine by flix
 * See http://wiki.wesnoth.org/AI_Recruitment
 */

#ifndef AI_DEFAULT_RECRUITMENT_HPP_INCLUDED
#define AI_DEFAULT_RECRUITMENT_HPP_INCLUDED

#include "../composite/rca.hpp"
#include "../../unit.hpp"
#include "../../unit_map.hpp"

#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <iomanip>

#ifdef _MSC_VER
#pragma warning(push)
// silence "inherits via dominance" warnings
#pragma warning(disable:4250)
#endif

namespace pathfind {

struct full_cost_map;

} //of namespace pathfind

namespace ai {

namespace default_recruitment {

// each leader have a score_map
// the score map indicates what is the best mix of own units from the leader point of view.
// The leader will then recruit according to the map.
typedef std::map<std::string, double> score_map;

typedef std::map<t_translation::t_terrain, int> terrain_count_map;
typedef std::map<std::string, int> count_map;

struct data {
	unit_map::const_iterator leader;
	std::set<std::string> recruits;
	score_map scores;

	// We use ratio_score to decide with which ratios the leaders recruit among each other.
	// For example if leader1 have a ratio_score of 1 and leader2 have a ratio_score of 2
	// then leader2 will recruit twice as much units than leader1.
	double ratio_score;

	int recruit_count;
	bool in_danger;

	explicit data(const unit_map::const_iterator leader)
		: leader(leader), ratio_score(1.0), recruit_count(0), in_danger(false) { }
	double get_score_sum() const {
		double sum = 0.0;
		BOOST_FOREACH(const score_map::value_type& entry, scores) {
			sum += entry.second;
		}
		return sum;
	}
	score_map get_normalized_scores() const {
		const double sum = get_score_sum();
		if (sum == 0.0) {
			return scores;
		}
		score_map normalized;
		BOOST_FOREACH(const score_map::value_type& entry, scores) {
			normalized[entry.first] = entry.second / sum;
		}
		return normalized;
	}
	std::string to_string() const {
		std::stringstream s;
		s << "---------------Content of leader data---------------\n";
		s << "For leader: " << leader->name() << "\n";
		s << "ratio_score: " << ratio_score << "\n";
		s << "recruit_count: " << recruit_count << "\n\n";
		BOOST_FOREACH(const score_map::value_type& entry, scores) {
			s << std::setw(20) << entry.first <<
					" score: " << std::setw(7) << entry.second << "\n";
		}
		s << "----------------------------------------------------\n";
		return s.str();
	}
};

struct cached_combat_value {
	double a_defense;
	double b_defense;
	double value;
	cached_combat_value(double a_def, double b_def, double v) :
		a_defense(a_def), b_defense(b_def), value(v) {
	}
	bool operator<(const cached_combat_value& o) const {
		return value < o.value;
	}
};

typedef std::map<std::string, std::set<cached_combat_value> > table_row;
typedef std::map<std::string, table_row> cache_table;

class recruitment : public candidate_action {
public:
	recruitment(rca_context &context, const config &cfg);
	virtual ~recruitment() { }
	virtual double evaluate();
	virtual void execute();
	config to_config() const;
private:
// Helper functions for execute()
	action_result_ptr execute_recall(const std::string& id, data& leader_data);
	action_result_ptr execute_recruit(const std::string& type, data& leader_data);
	const std::string* get_appropriate_recall(const std::string& type,
			const data& leader_data) const;
	data* get_best_leader_from_ratio_scores(std::vector<data>& leader_data,
			const config* job) const;
	const std::string get_best_recruit_from_scores(const data& leader_data,
			const config* job);

// Map Analysis
	void compare_cost_maps_and_update_important_hexes(
			const pathfind::full_cost_map& my_cost_map,
			const pathfind::full_cost_map& enemy_cost_map);
	double get_average_defense(const std::string& unit_type) const;
	const pathfind::full_cost_map get_cost_map_of_side(int side) const;
	void show_important_hexes() const;  //Debug only
	void update_average_lawful_bonus();
	void update_average_local_cost();
	void update_important_hexes();

// Combat Analysis
	double compare_unit_types(const std::string& a, const std::string& b);
	void do_combat_analysis(std::vector<data>* leader_data);
	const double* get_cached_combat_value(const std::string& a, const std::string& b,
			double a_defense, double b_defense);
	void simulate_attack(
			const unit_type* const attacker, const unit_type* const defender,
			double attacker_defense, double defender_defense,
			double* damage_to_attacker, double* damage_to_defender) const;

// Aspect recruitment_instruction
	config* get_most_important_job();
	const std::string get_random_pattern_type_if_exists(const data& leader_data,
			const config* job) const;
	void integrate_recruitment_pattern_in_recruitment_instructions();
	bool leader_matches_job(const data& leader_data, const config* job) const;
	bool limit_ok(const std::string& recruit) const;
	bool recruit_matches_job(const std::string& recruit, const config* job) const;
	bool recruit_matches_type(const std::string& recruit, const std::string& type) const;
	bool recruit_matches_types(const std::string& recruit,
			const std::vector<std::string>& types) const;
	bool remove_job_if_no_blocker(config* job);

// Aspect recruitment_save_gold
	double get_estimated_income(int turns) const;
	double get_estimated_unit_gain() const;
	double get_estimated_village_gain() const;
	double get_unit_ratio() const;
	void update_state();

// Other
	void do_randomness(std::vector<data>* leader_data) const;
	void do_similarity_penalty(std::vector<data>* leader_data) const;
	int get_cheapest_unit_cost_for_leader(const unit_map::const_iterator& leader);
	void handle_recruitment_more(std::vector<data>* leader_data) const;
	bool is_enemy_in_radius(const map_location& loc, int radius) const;
	void update_own_units_count();
	void update_scouts_wanted();

// Observer
	class recruit_situation_change_observer : public events::observer {
	public:
		recruit_situation_change_observer();
		~recruit_situation_change_observer();

		void handle_generic_event(const std::string& event);

		bool recruit_list_changed();
		void set_recruit_list_changed(bool changed);
		int gamestate_changed();
		void reset_gamestate_changed();

	private:
		bool recruit_list_changed_;
		int gamestate_changed_;

	};

	std::set<map_location> important_hexes_;
	terrain_count_map important_terrain_;
	int own_units_in_combat_counter_;
	std::map<map_location, double> average_local_cost_;
	std::map<size_t, int> cheapest_unit_costs_;
	cache_table combat_cache_;
	enum states {NORMAL, SAVE_GOLD, SPEND_ALL_GOLD, LEADER_IN_DANGER};
	states state_;
	recruit_situation_change_observer recruit_situation_change_observer_;
	int average_lawful_bonus_;
	config recruitment_instructions_;
	int recruitment_instructions_turn_;
	count_map own_units_count_;
	int total_own_units_;
	int scouts_wanted_;
};

}  // of namespace default_recruitment

}  // of namespace ai

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
