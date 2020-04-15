/*************************************************************************/
/*  animation.h                                                          */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#ifndef ANIMATION_H
#define ANIMATION_H

#include "core/resource.h"

class Animation : public Resource {

	GDCLASS(Animation, Resource);
	RES_BASE_EXTENSION("anim");

public:
	enum TrackType {
		TYPE_VALUE, ///< Set a value in a property, can be interpolated.
		TYPE_TRANSFORM, ///< Transform a node or a bone.
		TYPE_METHOD, ///< Call any method on a specific node.
		TYPE_BEZIER, ///< Bezier curve
		TYPE_AUDIO,
		TYPE_ANIMATION,
	};

	enum InterpolationType {
		INTERPOLATION_NEAREST,
		INTERPOLATION_LINEAR,
		INTERPOLATION_CUBIC
	};

	enum UpdateMode {
		UPDATE_CONTINUOUS,
		UPDATE_DISCRETE,
		UPDATE_TRIGGER,
		UPDATE_CAPTURE,

	};

private:
	struct Track {

		TrackType type;
		InterpolationType interpolation;
		bool loop_wrap;
		NodePath path; // path to something
		bool imported;
		bool enabled;
		Track() {
			interpolation = INTERPOLATION_LINEAR;
			imported = false;
			loop_wrap = true;
			enabled = true;
		}
		virtual ~Track() {}
	};

	struct Key {

		real_t transition;
		real_t time; // Time in seconds.
		Key() {
			transition = 1;
			time = 0;
		}
	};

	// transform key holds either Vector3 or Quaternion
	template <class T>
	struct TKey : public Key {

		T value;
	};

	struct TransformKey {

		Vector3 loc;
		Quat rot;
		Vector3 scale;
	};

	/* TRANSFORM TRACK */

	struct TransformTrack : public Track {

		Vector<TKey<TransformKey>> transforms;

		TransformTrack() { type = TYPE_TRANSFORM; }
	};

	/* PROPERTY VALUE TRACK */

	struct ValueTrack : public Track {

		UpdateMode update_mode;
		bool update_on_seek;
		Vector<TKey<Variant>> values;

		ValueTrack() {
			type = TYPE_VALUE;
			update_mode = UPDATE_CONTINUOUS;
		}
	};

	/* METHOD TRACK */

	struct MethodKey : public Key {

		StringName method;
		Vector<Variant> params;
	};

	struct MethodTrack : public Track {

		Vector<MethodKey> methods;
		MethodTrack() { type = TYPE_METHOD; }
	};

	/* BEZIER TRACK */

	struct BezierKey {
		Vector2 in_handle; //relative (x always <0)
		Vector2 out_handle; //relative (x always >0)
		real_t value;
	};

	struct BezierTrack : public Track {

		Vector<TKey<BezierKey>> values;

		BezierTrack() {
			type = TYPE_BEZIER;
		}
	};

	/* AUDIO TRACK */

	struct AudioKey {
		RES stream;
		real_t start_offset; //offset from start
		real_t end_offset; //offset from end, if 0 then full length or infinite
		AudioKey() {
			start_offset = 0;
			end_offset = 0;
		}
	};

	struct AudioTrack : public Track {

		Vector<TKey<AudioKey>> values;

		AudioTrack() {
			type = TYPE_AUDIO;
		}
	};

	/* AUDIO TRACK */

	struct AnimationTrack : public Track {

		Vector<TKey<StringName>> values;

		AnimationTrack() {
			type = TYPE_ANIMATION;
		}
	};

	Vector<Track *> tracks;

	/*
	template<class T>
	int _insert_pos(real_t p_time, T& p_keys);*/

	template <class T>
	void _clear(T &p_keys);

	template <class T, class V>
	int _insert(real_t p_time, T &p_keys, const V &p_value);

	template <class K>
	inline int _find(const Vector<K> &p_keys, real_t p_time) const;

	_FORCE_INLINE_ Animation::TransformKey _interpolate(const Animation::TransformKey &p_a, const Animation::TransformKey &p_b, real_t p_c) const;

	_FORCE_INLINE_ Vector3 _interpolate(const Vector3 &p_a, const Vector3 &p_b, real_t p_c) const;
	_FORCE_INLINE_ Quat _interpolate(const Quat &p_a, const Quat &p_b, real_t p_c) const;
	_FORCE_INLINE_ Variant _interpolate(const Variant &p_a, const Variant &p_b, real_t p_c) const;
	_FORCE_INLINE_ real_t _interpolate(const real_t &p_a, const real_t &p_b, real_t p_c) const;

	_FORCE_INLINE_ Animation::TransformKey _cubic_interpolate(const Animation::TransformKey &p_pre_a, const Animation::TransformKey &p_a, const Animation::TransformKey &p_b, const Animation::TransformKey &p_post_b, real_t p_c) const;
	_FORCE_INLINE_ Vector3 _cubic_interpolate(const Vector3 &p_pre_a, const Vector3 &p_a, const Vector3 &p_b, const Vector3 &p_post_b, real_t p_c) const;
	_FORCE_INLINE_ Quat _cubic_interpolate(const Quat &p_pre_a, const Quat &p_a, const Quat &p_b, const Quat &p_post_b, real_t p_c) const;
	_FORCE_INLINE_ Variant _cubic_interpolate(const Variant &p_pre_a, const Variant &p_a, const Variant &p_b, const Variant &p_post_b, real_t p_c) const;
	_FORCE_INLINE_ real_t _cubic_interpolate(const real_t &p_pre_a, const real_t &p_a, const real_t &p_b, const real_t &p_post_b, real_t p_c) const;

	template <class T>
	_FORCE_INLINE_ T _interpolate(const Vector<TKey<T>> &p_keys, real_t p_time, InterpolationType p_interp, bool p_loop_wrap, bool *p_ok) const;

	template <class T>
	_FORCE_INLINE_ void _track_get_key_indices_in_range(const Vector<T> &p_array, real_t from_time, real_t to_time, List<int> *p_indices) const;

	_FORCE_INLINE_ void _value_track_get_key_indices_in_range(const ValueTrack *vt, real_t from_time, real_t to_time, List<int> *p_indices) const;
	_FORCE_INLINE_ void _method_track_get_key_indices_in_range(const MethodTrack *mt, real_t from_time, real_t to_time, List<int> *p_indices) const;

	real_t length;
	real_t step;
	bool loop;

	// bind helpers
private:
	Array _transform_track_interpolate(int p_track, real_t p_time) const {
		Vector3 loc;
		Quat rot;
		Vector3 scale;
		transform_track_interpolate(p_track, p_time, &loc, &rot, &scale);
		Array ret;
		ret.push_back(loc);
		ret.push_back(rot);
		ret.push_back(scale);
		return ret;
	}

	Vector<int> _value_track_get_key_indices(int p_track, real_t p_time, real_t p_delta) const {

		List<int> idxs;
		value_track_get_key_indices(p_track, p_time, p_delta, &idxs);
		Vector<int> idxr;

		for (List<int>::Element *E = idxs.front(); E; E = E->next()) {

			idxr.push_back(E->get());
		}
		return idxr;
	}
	Vector<int> _method_track_get_key_indices(int p_track, real_t p_time, real_t p_delta) const {

		List<int> idxs;
		method_track_get_key_indices(p_track, p_time, p_delta, &idxs);
		Vector<int> idxr;

		for (List<int>::Element *E = idxs.front(); E; E = E->next()) {

			idxr.push_back(E->get());
		}
		return idxr;
	}

	bool _transform_track_optimize_key(const TKey<TransformKey> &t0, const TKey<TransformKey> &t1, const TKey<TransformKey> &t2, real_t p_alowed_linear_err, real_t p_alowed_angular_err, real_t p_max_optimizable_angle, const Vector3 &p_norm);
	void _transform_track_optimize(int p_idx, real_t p_allowed_linear_err = 0.05, real_t p_allowed_angular_err = 0.01, real_t p_max_optimizable_angle = Math_PI * 0.125);

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	static void _bind_methods();

public:
	int add_track(TrackType p_type, int p_at_pos = -1);
	void remove_track(int p_track);

	int get_track_count() const;
	TrackType track_get_type(int p_track) const;

	void track_set_path(int p_track, const NodePath &p_path);
	NodePath track_get_path(int p_track) const;
	int find_track(const NodePath &p_path) const;
	// transform

	void track_move_up(int p_track);
	void track_move_down(int p_track);
	void track_move_to(int p_track, int p_to_index);
	void track_swap(int p_track, int p_with_track);

	void track_set_imported(int p_track, bool p_imported);
	bool track_is_imported(int p_track) const;

	void track_set_enabled(int p_track, bool p_enabled);
	bool track_is_enabled(int p_track) const;

	void track_insert_key(int p_track, real_t p_time, const Variant &p_key, real_t p_transition = 1);
	void track_set_key_transition(int p_track, int p_key_idx, real_t p_transition);
	void track_set_key_value(int p_track, int p_key_idx, const Variant &p_value);
	void track_set_key_time(int p_track, int p_key_idx, real_t p_time);
	int track_find_key(int p_track, real_t p_time, bool p_exact = false) const;
	void track_remove_key(int p_track, int p_idx);
	void track_remove_key_at_position(int p_track, real_t p_pos);
	int track_get_key_count(int p_track) const;
	Variant track_get_key_value(int p_track, int p_key_idx) const;
	real_t track_get_key_time(int p_track, int p_key_idx) const;
	real_t track_get_key_transition(int p_track, int p_key_idx) const;

	int transform_track_insert_key(int p_track, real_t p_time, const Vector3 &p_loc, const Quat &p_rot = Quat(), const Vector3 &p_scale = Vector3());
	Error transform_track_get_key(int p_track, int p_key, Vector3 *r_loc, Quat *r_rot, Vector3 *r_scale) const;
	void track_set_interpolation_type(int p_track, InterpolationType p_interp);
	InterpolationType track_get_interpolation_type(int p_track) const;

	int bezier_track_insert_key(int p_track, real_t p_time, real_t p_value, const Vector2 &p_in_handle, const Vector2 &p_out_handle);
	void bezier_track_set_key_value(int p_track, int p_index, real_t p_value);
	void bezier_track_set_key_in_handle(int p_track, int p_index, const Vector2 &p_handle);
	void bezier_track_set_key_out_handle(int p_track, int p_index, const Vector2 &p_handle);
	real_t bezier_track_get_key_value(int p_track, int p_index) const;
	Vector2 bezier_track_get_key_in_handle(int p_track, int p_index) const;
	Vector2 bezier_track_get_key_out_handle(int p_track, int p_index) const;

	real_t bezier_track_interpolate(int p_track, real_t p_time) const;

	int audio_track_insert_key(int p_track, real_t p_time, const RES &p_stream, real_t p_start_offset = 0, real_t p_end_offset = 0);
	void audio_track_set_key_stream(int p_track, int p_key, const RES &p_stream);
	void audio_track_set_key_start_offset(int p_track, int p_key, real_t p_offset);
	void audio_track_set_key_end_offset(int p_track, int p_key, real_t p_offset);
	RES audio_track_get_key_stream(int p_track, int p_key) const;
	real_t audio_track_get_key_start_offset(int p_track, int p_key) const;
	real_t audio_track_get_key_end_offset(int p_track, int p_key) const;

	int animation_track_insert_key(int p_track, real_t p_time, const StringName &p_animation);
	void animation_track_set_key_animation(int p_track, int p_key, const StringName &p_animation);
	StringName animation_track_get_key_animation(int p_track, int p_key) const;

	void track_set_interpolation_loop_wrap(int p_track, bool p_enable);
	bool track_get_interpolation_loop_wrap(int p_track) const;

	Error transform_track_interpolate(int p_track, real_t p_time, Vector3 *r_loc, Quat *r_rot, Vector3 *r_scale) const;

	Variant value_track_interpolate(int p_track, real_t p_time) const;
	void value_track_get_key_indices(int p_track, real_t p_time, real_t p_delta, List<int> *p_indices) const;
	void value_track_set_update_mode(int p_track, UpdateMode p_mode);
	UpdateMode value_track_get_update_mode(int p_track) const;

	void method_track_get_key_indices(int p_track, real_t p_time, real_t p_delta, List<int> *p_indices) const;
	Vector<Variant> method_track_get_params(int p_track, int p_key_idx) const;
	StringName method_track_get_name(int p_track, int p_key_idx) const;

	void copy_track(int p_track, Ref<Animation> p_to_animation);

	void track_get_key_indices_in_range(int p_track, real_t p_time, real_t p_delta, List<int> *p_indices) const;

	void set_length(real_t p_length);
	real_t get_length() const;

	void set_loop(bool p_enabled);
	bool has_loop() const;

	void set_step(real_t p_step);
	real_t get_step() const;

	void clear();

	void optimize(real_t p_allowed_linear_err = 0.05, real_t p_allowed_angular_err = 0.01, real_t p_max_optimizable_angle = Math_PI * 0.125);

	Animation();
	~Animation();
};

VARIANT_ENUM_CAST(Animation::TrackType);
VARIANT_ENUM_CAST(Animation::InterpolationType);
VARIANT_ENUM_CAST(Animation::UpdateMode);

#endif
