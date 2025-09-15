from dearpygui import dearpygui as dpg
from matplotlib import pyplot as plt
import numpy as np

dpg.create_context()

def on_button_click(sender, app_data):
    entered_text = dpg.get_value(input_field)
    dpg.set_value(input_field, f"You Typed {entered_text}")

with dpg.window(label="My First Gui"):
    dpg.add_text("Hello World")

    input_field = dpg.add_input_text(label="Enter something:",
                                     default_value="Type here...")
    dpg.add_button(label="Submit", callback=on_button_click)

dpg.create_viewport(title="Hello World App", width=400, height=300)

dpg.setup_dearpygui()
dpg.show_viewport()
dpg.start_dearpygui()


dpg.destroy_context()

