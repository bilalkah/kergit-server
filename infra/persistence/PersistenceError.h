#ifndef INFRA_PERSISTENCE_PERSISTENCE_ERROR_H
#define INFRA_PERSISTENCE_PERSISTENCE_ERROR_H

#include <stdexcept>

// Error vocabulary for the persistence layer.
//
// The point of these two types is to answer one question at every call site:
// "is this a database hiccup that heals itself, or a real failure?"
//
//   - TransientPersistenceError -> the connection layer broke and the pool
//     recovers on its own (DB restarted, idle socket dropped, pool momentarily
//     exhausted). The operation did not complete, but the server is healthy and
//     a later attempt is expected to succeed. Callers should treat it as a blip.
//
//   - any other exception (PersistenceError, or a raw pqxx::sql_error such as a
//     constraint violation) -> a genuine failure that will not fix itself.
//
// Only connection/availability failures are mapped to TransientPersistenceError
// by TransactionRunner; SQL errors propagate unchanged so real bugs are never
// masked as "transient".
class PersistenceError : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};

// A connection-level failure that the pool heals automatically. Safe to treat as
// a temporary condition and retry later.
class TransientPersistenceError : public PersistenceError {
   public:
    using PersistenceError::PersistenceError;
};

#endif  // INFRA_PERSISTENCE_PERSISTENCE_ERROR_H
