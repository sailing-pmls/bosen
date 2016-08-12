// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.02.14

#pragma once


namespace petuum {

/**
 * The Lockable concept (implemented as interface/abstract class) describes
 * the characteristics of types that provide exclusive blocking semantics for
 * execution agents (i.e. threads).
 */
class Lockable {
public:
  /**
   * Blocks until a lock can be obtained for the current execution agent. If
   * an exception is thrown, no lock is obtained.
   */
  virtual void lock() = 0;

  /**
   * Releases the lock held by the execution agent. Throws no exceptions.
   * requires: The current execution agent should hold the lock.
   */
  virtual void unlock() = 0;

  /**
   * Attempts to acquire the lock for the current execution agent without
   * blocking. If an exception is thrown, no lock is obtained.
   * @return true if the lock was acquired, false otherwise
   */
  virtual bool try_lock() = 0;

};

}   // namespace petuum
