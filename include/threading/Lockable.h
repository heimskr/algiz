#pragma once

#include <concepts>
#include <format>
#include <mutex>
#include <shared_mutex>

namespace Algiz {
	using DefaultMutex = std::shared_mutex;

	template <typename T, typename M = DefaultMutex>
	struct Lockable: T {
		using Base = T;

		mutable M mutex;

		using T::T;

		Lockable(const Lockable<T, M> &other): T(other.getBase()) {}
		Lockable(const T &other): T(other) {}

		/** Likely data race issue. */
		Lockable(Lockable<T, M> &&other) noexcept: T(std::move(other.getBase())) {}
		Lockable(T &&other) noexcept: T(std::move(other)) {}

		Lockable<T, M> & operator=(const Lockable<T, M> &other) {
			if (this == &other)
				return *this;
			auto this_lock = uniqueLock();
			auto other_lock = other.sharedLock();
			T::operator=(other.getBase());
			return *this;
		}

		Lockable<T, M> & operator=(Lockable<T, M> &&other) noexcept {
			if (this == &other)
				return *this;
			auto this_lock = uniqueLock();
			auto other_lock = other.uniqueLock();
			T::operator=(std::move(other.getBase()));
			return *this;
		}

		template <typename U>
		Lockable<T, M> & operator=(const U &other) {
			auto lock = uniqueLock();
			T::operator=(other);
			return *this;
		}

		template <typename U>
		Lockable<T, M> & operator=(U &&other) noexcept {
			auto lock = uniqueLock();
			T::operator=(std::forward<U>(other));
			return *this;
		}

		template <typename U>
		Lockable<T, M> & unsafeSet(U &&other) {
			T::operator=(std::forward<U>(other));
			return *this;
		}

		inline auto uniqueLock() const { return std::unique_lock(mutex); }
		inline auto sharedLock() const { return std::shared_lock(mutex); }
		inline auto tryUniqueLock() const { return std::unique_lock(mutex, std::try_to_lock); }
		inline auto trySharedLock() const { return std::shared_lock(mutex, std::try_to_lock); }

		inline T & getBase() { return static_cast<T &>(*this); }
		inline const T & getBase() const { return static_cast<const T &>(*this); }

		inline T & getBase(std::shared_lock<std::shared_mutex> &lock) { lock = sharedLock(); return static_cast<T &>(*this); }
		inline T & getBase(std::unique_lock<std::shared_mutex> &lock) { lock = uniqueLock(); return static_cast<T &>(*this); }

		inline const T & getBase(std::shared_lock<std::shared_mutex> &lock) const { lock = sharedLock(); return static_cast<const T &>(*this); }
		inline const T & getBase(std::unique_lock<std::shared_mutex> &lock) const { lock = uniqueLock(); return static_cast<const T &>(*this); }

		template <typename Fn>
		requires std::invocable<Fn>
		decltype(auto) withShared(Fn &&function) const {
			auto lock = sharedLock();
			return function();
		}

		template <typename Fn>
		requires std::invocable<Fn, const T &>
		decltype(auto) withShared(Fn &&function) const {
			auto lock = sharedLock();
			return function(getBase());
		}

		template <typename Fn>
		requires (std::invocable<Fn, T &> && !std::invocable<Fn, const T &>)
		decltype(auto) withShared(Fn &&function) {
			auto lock = sharedLock();
			return function(getBase());
		}

		template <typename Fn>
		requires std::invocable<Fn>
		decltype(auto) withUnique(Fn &&function) const {
			auto lock = uniqueLock();
			return function();
		}

		template <typename Fn>
		requires std::invocable<Fn, const T &>
		decltype(auto) withUnique(Fn &&function) const {
			auto lock = uniqueLock();
			return function(getBase());
		}

		template <typename Fn>
		requires (std::invocable<Fn, T &> && !std::invocable<Fn, const T &>)
		decltype(auto) withUnique(Fn &&function) {
			auto lock = uniqueLock();
			return function(getBase());
		}

		inline T copyBase() const {
			auto lock = sharedLock();
			return static_cast<T>(*this);
		}

		inline T swap(T value) {
			auto lock = uniqueLock();
			std::swap(getBase(), value);
			return value;
		}
	};
}

template <typename T>
struct std::formatter<Algiz::Lockable<T>> {
	constexpr auto parse(auto &ctx) {
		return ctx.begin();
	}

	auto format(const auto &lockable, auto &ctx) const {
		auto lock = lockable.sharedLock();
		return std::format_to(ctx.out(), "{}", lockable.getBase());
	}
};
