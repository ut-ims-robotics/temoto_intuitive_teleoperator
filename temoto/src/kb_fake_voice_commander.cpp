#include "ros/ros.h"
#include "std_msgs/String.h"
//#include "temoto/Command.h"
#include "keyboard_reader/Key.h"

class fake_voice_commander
{
public:
    ros::NodeHandle n;
    ros::Publisher voiceCommandPublisher;
    ros::Subscriber sub_kb_event;
    std_msgs::String voiceCommand;

    fake_voice_commander()
    {
        // Setup ROS publishers
        //voiceCommandPublisher = n.advertise<temoto::Command>("temoto/voice_commands", 2);
        voiceCommandPublisher = n.advertise<std_msgs::String>("pocketsphinx_recognizer/output", 2);

        // Setup ROS subscribers
        sub_kb_event = n.subscribe<keyboard_reader::Key>("keyboard", 1, &fake_voice_commander::keyboardCallback, this);
    }

    // Process keypresses
    void keyboardCallback(keyboard_reader::Key kbCommand)
    {
        if (kbCommand.key_pressed == true)
        {
            // Plan trajecory: "p" key
            if (kbCommand.key_code == 0x0019 )
            {
                voiceCommand.data = "robot please plan";
            }

            // Execute trajectory: "e" key
            else if (kbCommand.key_code == 0x0012)
            {
                voiceCommand.data = "robot please execute";
            }

            // Natural perspective: "n" key
            else if (kbCommand.key_code == 0x0031)
            {
                voiceCommand.data = "natural control mode";
            }

            // Inverted perspective: "i" key
            else if (kbCommand.key_code == 0x0017)
            {
                voiceCommand.data = "inverted control mode";
            }

            // Limit directions: "l" key
            else if (kbCommand.key_code == 0x0026)
            {
                voiceCommand.data = "limit directions";
            }

            // Free directions: "f" key
            else if (kbCommand.key_code == 0x0021)
            {
                voiceCommand.data = "free directions";
            }

            ROS_INFO("[fake voice commander]Publishing: %s", voiceCommand.data.c_str());
            voiceCommandPublisher.publish(voiceCommand);
        }
    }
};

int main(int argc, char **argv)
{
    // ROS init
    ros::init(argc, argv, "fake_voice_commander");
    ROS_INFO("Fake voice commander up and running");
    fake_voice_commander fakeVoiceCommander;

    ros::spin();

    return 0;
} // end main
