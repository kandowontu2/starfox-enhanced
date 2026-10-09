package com.starfox.enhanced.quest;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

public final class QuestSessionGateTest {
    private static void require(boolean value) {
        if (!value) throw new AssertionError("Quest session ownership failed");
    }
    public static void main(String[] args) throws Exception {
        SessionGate gate = new SessionGate();
        SessionGate.Lease first = gate.tryAcquire();
        require(first != null && gate.tryAcquire() == null);
        AtomicInteger cleanups = new AtomicInteger();
        first.close(cleanups::incrementAndGet);
        SessionGate.Lease second = gate.tryAcquire();
        require(second != null);
        first.close(cleanups::incrementAndGet); // Stale shutdown cannot release second.
        require(cleanups.get() == 1 && gate.tryAcquire() == null);
        try {
            second.close(() -> { throw new IllegalStateException("Injected cleanup failure"); });
            throw new AssertionError("Cleanup failure was hidden");
        } catch (IllegalStateException expected) {}
        SessionGate.Lease third = gate.tryAcquire();
        require(third != null);
        CountDownLatch entered = new CountDownLatch(1), finish = new CountDownLatch(1);
        Thread worker = new Thread(() -> third.close(() -> {
            entered.countDown();
            try {require(finish.await(5, TimeUnit.SECONDS));} catch (InterruptedException error) {throw new AssertionError(error);}
        }));
        worker.setDaemon(true);
        worker.start();require(entered.await(5, TimeUnit.SECONDS));
        require(gate.tryAcquire() == null); // Cleanup is still using SDL.
        third.close(cleanups::incrementAndGet);
        require(gate.tryAcquire() == null);
        finish.countDown();worker.join(5000);require(!worker.isAlive());
        SessionGate.Lease fourth = gate.tryAcquire();
        require(fourth != null);
        fourth.close(cleanups::incrementAndGet); // Startup-failure path, no worker.
        require(cleanups.get() == 2 && gate.tryAcquire() != null);
        System.out.println("Quest session gate: exclusivity, cleanup failure, stale release and in-flight cleanup passed");
    }
}
