// Development tool: reproduces pluginval's "Editor Automation" test without a
// plugin host, so the whole program can be built with sanitizers.
// Editor open in a real window; one thread sets every parameter to random
// values 1000x (10 ms apart), another runs audio, the message loop runs.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <thread>

static void onCrash (void*)
{
    std::printf ("\n*** CRASH in ArroyoStress. Backtrace:\n%s\n", juce::SystemStats::getStackBacktrace().toRawUTF8());
    std::fflush (stdout);
    std::_Exit (3);
}

int main (int argc, char** argv)
{
    juce::SystemStats::setApplicationCrashHandler (onCrash);
    juce::ScopedJuceInitialiser_GUI init;
    const bool fromMessageThread = argc > 1 && juce::String (argv[1]) == "message";
    const int iterations = argc > 2 ? juce::String (argv[2]).getIntValue() : 1000;

    ArroyoProcessor p;
    p.setPlayConfigDetails (2, 2, 44100.0, 512);
    p.prepareToPlay (44100.0, 512);

    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
   #if JUCE_WINDOWS
    ed->addToDesktop (0); // real window, real Direct2D rendering
   #endif
    ed->setVisible (true);
    // No window manager on the virtual display: repaint the editor into an
    // image ~30x per second instead, exercising every paint() path.
    struct Painter : juce::Timer
    {
        juce::Component* c = nullptr;
        int frames = 0;
        void timerCallback() override { c->createComponentSnapshot (c->getLocalBounds()); ++frames; }
    } painter;
    painter.c = ed.get();
    painter.startTimerHz (30);

    std::atomic<bool> done { false };
    std::atomic<int> count { 0 };
    auto randomise = [&p] (juce::Random& r)
    {
        for (auto* prm : p.getParameters())
            prm->setValueNotifyingHost (r.nextFloat());
    };

    std::thread audio ([&]
    {
        juce::AudioBuffer<float> b (2, 512);
        juce::MidiBuffer m;
        juce::Random r (2);
        while (! done)
        {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                    b.setSample (c, i, r.nextFloat() * 2.0f - 1.0f);
            p.processBlock (b, m);
            juce::Thread::sleep (3);
        }
    });

    std::thread bg;
    struct Driver : juce::Timer
    {
        std::function<void()> fn;
        void timerCallback() override { fn(); }
    } driver;
    juce::Random rm (1);

    if (fromMessageThread)
    {
        driver.fn = [&] { randomise (rm); if (++count >= iterations) done = true; };
        driver.startTimer (10);
    }
    else
    {
        bg = std::thread ([&]
        {
            juce::Random r (1);
            for (int n = 0; n < iterations; ++n)
            {
                randomise (r);
                juce::Thread::sleep (10);
            }
            done = true;
        });
    }

    while (! done)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (20);

    driver.stopTimer();
    painter.stopTimer();
    std::printf ("frames painted: %d\n", painter.frames);
    if (bg.joinable()) bg.join();
    audio.join();
    // let pending async updates drain, then close the editor
    juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
    ed.reset();
    std::printf ("stress (%s thread, %d iterations): completed\n", fromMessageThread ? "message" : "background", iterations);
    return 0;
}
