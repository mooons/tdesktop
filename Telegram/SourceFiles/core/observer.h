/*
This file is part of Telegram Desktop,
the official desktop version of Telegram messaging app, see https://telegram.org

Telegram Desktop is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

It is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

In addition, as a special exception, the copyright holders give permission
to link the code of portions of this program with the OpenSSL library.

Full license: https://github.com/telegramdesktop/tdesktop/blob/master/LICENSE
Copyright (c) 2014-2016 John Preston, https://desktop.telegram.org
*/
#pragma once

#include "core/vector_of_moveable.h"

namespace Notify {

class Observer;
using ConnectionId = uint32;

// Each observer type should have observerRegistered(Notify::ConnectionId connection) method.
// Usually it is done by deriving the type from the Notify::Observer base class.
// In destructor it should call Notify::unregisterObserver(connection) for all the connections.

namespace internal {
void observerRegisteredDefault(Observer *observer, ConnectionId connection);
} // namespace internal

void unregisterObserver(ConnectionId connection);

class Observer {
public:
	virtual ~Observer() = 0;

private:
	void observerRegistered(ConnectionId connection);
	friend void internal::observerRegisteredDefault(Observer *observer, ConnectionId connection);

	QVector<ConnectionId> _connections;

};

using ObservedEvent = uchar;
inline ConnectionId observerConnectionId(ObservedEvent event, int connectionIndex) {
	t_assert(connectionIndex >= 0 && connectionIndex < 0x01000000);
	return (static_cast<uint32>(event) << 24) | (connectionIndex + 1);
}

using UnregisterObserverCallback = void(*)(int connectionIndex);

// Usage: UnregisterObserverCallbackCreator creator(myEvent, myCallback); in global scope.
class UnregisterObserverCallbackCreator {
public:
	UnregisterObserverCallbackCreator(ObservedEvent event, UnregisterObserverCallback callback);

};

// Handler is one of Function<> instantiations.
template <typename Flags, typename Handler>
struct ObserversList {
	struct Entry {
		Flags flags;
		Handler handler;
	};
	std_::vector_of_moveable<Entry> entries;
	QVector<int> freeIndices;
};

template <typename Flags, typename Handler>
int registerObserver(ObserversList<Flags, Handler> &list, Flags flags, Handler &&handler) {
	while (!list.freeIndices.isEmpty()) {
		auto freeIndex = list.freeIndices.back();
		list.freeIndices.pop_back();

		if (freeIndex < list.entries.size()) {
			list.entries[freeIndex] = { flags, std_::move(handler) };
			return freeIndex;
		}
	}
	list.entries.push_back({ flags, std_::move(handler) });
	return list.entries.size() - 1;
}

template <typename Flags, typename Handler>
void unregisterObserver(ObserversList<Flags, Handler> &list, int connectionIndex) {
	auto &entries(list.entries);
	if (entries.size() <= connectionIndex) return;

	if (entries.size() == connectionIndex + 1) {
		for (entries.pop_back(); !entries.isEmpty() && entries.back().handler.isNull();) {
			entries.pop_back();
		}
	} else {
		entries[connectionIndex].handler = Handler();
		list.freeIndices.push_back(connectionIndex);
	}
}

template <typename Flags, typename Handler, typename... Args>
void notifyObservers(ObserversList<Flags, Handler> &list, Flags flags, Args&&... args) {
	for (auto &entry : list.entries) {
		if (!entry.handler.isNull() && (flags & entry.flags)) {
			entry.handler.call(std_::forward<Args>(args)...);
		}
	}
}

namespace internal {

template <typename ObserverType, int>
struct ObserverRegisteredGeneric {
	static void call(ObserverType *observer, ConnectionId connection) {
		observer->observerRegistered(connection);
	}
};

template<typename ObserverType>
struct ObserverRegisteredGeneric<ObserverType, true> {
	static void call(ObserverType *observer, ConnectionId connection) {
		observerRegisteredDefault(observer, connection);
	}
};

} // namespace internal
} // namespace Notify
