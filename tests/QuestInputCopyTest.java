package com.starfox.enhanced.quest;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Arrays;

public final class QuestInputCopyTest {
    interface Checked {void run() throws Exception;}
    static void fails(Checked action) throws Exception {
        try {action.run();} catch (IOException expected) {return;}
        throw new AssertionError("Expected import failure");
    }
    public static void main(String[] args) throws Exception {
        byte[] source = new byte[200003];
        for (int i = 0; i < source.length; i++) source[i] = (byte) (i * 17);
        ByteArrayOutputStream destination = new ByteArrayOutputStream();
        if (InputCopy.copy(new ByteArrayInputStream(source), destination, source.length) != source.length
            || !Arrays.equals(source, destination.toByteArray())) throw new AssertionError("Streaming copy differs");
        fails(() -> InputCopy.copy(new ByteArrayInputStream(source), new ByteArrayOutputStream(), source.length - 1));
        fails(() -> InputCopy.copy(new ByteArrayInputStream(new byte[0]), new ByteArrayOutputStream(), 100));
        InputStream zeroRead = new ByteArrayInputStream(source) {
            boolean zero = true;
            @Override public synchronized int read(byte[] bytes, int offset, int length) {
                if (zero) {zero = false;return 0;}
                return super.read(bytes, offset, length);
            }
        };
        destination.reset();InputCopy.copy(zeroRead, destination, source.length);
        if (!Arrays.equals(source, destination.toByteArray())) throw new AssertionError("Zero-read handling differs");
        fails(() -> InputCopy.copy(new InputStream() {
            @Override public int read() throws IOException {throw new IOException("Provider failed");}
        }, new ByteArrayOutputStream(), 10));
        fails(() -> InputCopy.copy(new ByteArrayInputStream(source), new OutputStream() {
            @Override public void write(int value) throws IOException {throw new IOException("Storage failed");}
        }, source.length));
        try {
            InputCopy.copy(new ByteArrayInputStream(source), new ByteArrayOutputStream(), 0);
            throw new AssertionError("Invalid limit accepted");
        } catch (IllegalArgumentException expected) {}
        System.out.println("Quest bounded input-copy tests passed");
    }
}
