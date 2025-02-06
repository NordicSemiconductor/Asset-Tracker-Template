#!/usr/bin/env python3
import json
import math

class ViewerGenerator:
    def __init__(self):
        # Define center coordinates as class variables
        self.center_x = 300
        self.center_y = 200
        
        # Full template_start with all HTML and CSS
        self.template_start = """<!DOCTYPE html>
<html>
<head>
    <title>State Machine Viewer</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 20px;
            background: #f0f0f0;
        }
        .container {
            max-width: 1400px;
            margin: 0 auto;
            background: white;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }
        .grid {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 20px;
            margin: 20px 0;
        }
        .module {
            border: 2px solid #ddd;
            border-radius: 8px;
            padding: 10px;
            background: white;
            min-height: 400px;
        }
        .module h2 {
            margin: 0 0 10px 0;
            font-size: 16px;
            color: #333;
        }
        .state-machine {
            position: relative;
            width: 100%;
            height: 400px;
            margin: 10px 0;
        }
        .state {
            position: absolute;
            width: 160px;
            height: 45px;
            border: 2px solid #666;
            border-radius: 6px;
            display: flex;
            align-items: center;
            justify-content: center;
            text-align: center;
            background: white;
            font-size: 12px;
            z-index: 2;
            cursor: pointer;
            transition: all 0.3s ease;
            user-select: none;
            padding: 0 5px;
        }
        .state.active {
            background: #4CAF50;
            color: white;
            border-color: #45a049;
        }
        .state-group {
            position: absolute;
            border: 2px dashed #666;
            border-radius: 10px;
            background: rgba(0,0,0,0.02);
        }
        .log {
            margin-top: 10px;
            padding: 5px;
            background: #333;
            color: #fff;
            border-radius: 4px;
            height: 60px;
            overflow-y: auto;
            font-size: 12px;
        }
        .controls {
            margin: 20px 0;
            padding: 10px;
            background: #e8f5e9;
            border-radius: 4px;
        }
        .control-group {
            margin: 10px 0;
            padding: 10px;
            border: 1px solid #ccc;
            border-radius: 4px;
        }
        .control-group h3 {
            margin: 0 0 10px 0;
            font-size: 14px;
            color: #333;
        }
        button {
            margin: 5px;
            padding: 8px 16px;
            border: none;
            border-radius: 4px;
            background: #4CAF50;
            color: white;
            cursor: pointer;
            font-family: monospace;
            font-size: 12px;
        }
        button:hover {
            background: #45a049;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>State Machine Viewer</h1>
"""
        
        # Full template_end with all JavaScript
        self.template_end = """
    </div>
    <script>
        const stateMachines = %s;

        function log(module, message) {
            const logDiv = document.getElementById(`${module}-log`);
            logDiv.innerHTML += `${new Date().toLocaleTimeString()} - ${message}<br>`;
            logDiv.scrollTop = logDiv.scrollHeight;
        }

        function updateState(module, newState) {
            const machine = stateMachines[module];
            
            // Remove active class from all states in this module
            document.querySelectorAll(`#${module} .state`).forEach(el => {
                el.classList.remove('active');
            });
            
            // Add active class to new state
            const stateEl = document.querySelector(`#${module} .state[data-state="${newState}"]`);
            if (stateEl) {
                stateEl.classList.add('active');
                machine.currentState = newState;
                log(module, `State changed to: ${newState}`);
                
                // If this is a child state, activate the parent
                const state = machine.states[newState];
                if (state.parent) {
                    const parentEl = document.querySelector(`#${module} .state[data-state="${state.parent}"]`);
                    if (parentEl) {
                        parentEl.classList.add('active');
                    }
                }
            }
        }

        function handleStateClick(module, state) {
            const machine = stateMachines[module];
            const currentState = machine.currentState;
            
            // Find valid transitions from current state
            const transition = machine.transitions.find(t => 
                t.from === currentState && t.to === state
            );
            
            if (transition) {
                log(module, `Transition triggered by: ${transition.triggers[0]}`);
                updateState(module, state);
            } else {
                log(module, `Invalid transition to: ${state}`);
            }
        }

        function triggerEvent(module, event) {
            const machine = stateMachines[module];
            const currentState = machine.currentState;
            
            // Find transition for this event
            const transition = machine.transitions.find(t => 
                t.from === currentState && t.triggers.includes(event)
            );
            
            if (transition) {
                log(module, `Event triggered: ${event}`);
                updateState(module, transition.to);
            } else {
                log(module, `Invalid event for current state: ${event}`);
            }
        }

        function resetAll() {
            Object.keys(stateMachines).forEach(module => {
                const machine = stateMachines[module];
                const initialState = Object.keys(machine.states)[0];
                updateState(module, initialState);
                
                // Handle automatic transition from RUNNING to WAIT_FOR_CLOUD
                if (module === 'fota' && initialState === 'RUNNING') {
                    const autoTransition = machine.transitions.find(t => 
                        t.from === 'RUNNING' && 
                        t.to === 'WAIT_FOR_CLOUD' && 
                        t.triggers.length === 0
                    );
                    if (autoTransition) {
                        setTimeout(() => {
                            updateState(module, 'WAIT_FOR_CLOUD');
                            log(module, 'Automatic transition to WAIT_FOR_CLOUD');
                        }, 500);
                    }
                }
                
                const logDiv = document.getElementById(`${module}-log`);
                logDiv.innerHTML = '';
                log(module, 'Reset to initial state');
            });
        }

        // Initialize all state machines
        resetAll();
    </script>
</body>
</html>"""

    def generate_state_positions(self, states):
        """Generate positions for states in a circular layout"""
        positions = {}
        num_states = len(states)
        radius = 120
        
        # Custom positioning for FOTA states
        if 'RUNNING' in states:  # This is the FOTA state machine
            state_positions = {
                'RUNNING': (self.center_x, 80),  # Top center
                'WAIT_FOR_CLOUD': (self.center_x + 120, self.center_y),  # Right
                'WAIT_FOR_TRIGGER': (self.center_x + 120, self.center_y + 120),  # Bottom right
                'POLL_AND_PROCESS': (self.center_x - 120, self.center_y + 120),  # Bottom left
                'REBOOT_PENDING': (self.center_x - 120, self.center_y),  # Left
            }
            return state_positions
        
        # Default circular layout for other state machines
        for i, state_name in enumerate(states):
            angle = (i * 2 * math.pi / num_states) - (math.pi / 2)  # Start from top
            x = self.center_x + math.cos(angle) * radius
            y = self.center_y + math.sin(angle) * radius
            positions[state_name] = (x, y)
            
        return positions

    def generate_html(self, json_file):
        """Generate HTML visualization from state machine JSON"""
        # Read state machine definitions
        with open(json_file) as f:
            state_machines = json.load(f)
        
        # Start with template
        html = [self.template_start]
        
        # Add controls section
        html.append('        <div class="controls">')
        
        # Generate event buttons for each state machine
        for machine_name, machine in state_machines.items():
            display_name = machine_name.upper()
            html.append(f'            <div class="control-group">')
            html.append(f'                <h3>{display_name} Events</h3>')
            
            # Add button for each event
            for event in machine['events']:
                html.append(f'                <button onclick="triggerEvent(\'{machine_name}\', \'{event}\')">{event}</button>')
            
            html.append('            </div>')
        
        # Add reset control
        html.append("""            <div class="control-group">
                <h3>System Control</h3>
                <button onclick="resetAll()">Reset All</button>
            </div>""")
        
        html.append('        </div>')
        
        # Add state machine visualizations
        html.append('        <div class="grid">')
        
        # Generate visualization for each state machine
        for machine_name, machine in state_machines.items():
            display_name = machine_name.upper()
            states = list(machine['states'].keys())
            positions = self.generate_state_positions(states)
            
            # Start machine div
            html.append(f"""            <div class="module" id="{machine_name}">
                <h2>{display_name}</h2>
                <div class="state-machine">""")
            
            # Add state group for child states
            if 'RUNNING' in states:  # FOTA machine
                html.append(f"""                    <div class="state-group" style="
                    left: {self.center_x - 180}px;
                    top: {self.center_y - 20}px;
                    width: 360px;
                    height: 180px;">
                </div>""")
            
            # Add states
            for state_name in states:
                x, y = positions[state_name]
                html.append(f"""                    <div class="state" 
                        data-state="{state_name}"
                        onclick="handleStateClick('{machine_name}', '{state_name}')"
                        style="left: {x-80}px; top: {y-22.5}px;">
                        {state_name}
                    </div>""")
            
            # Add log section
            html.append(f"""                </div>
                <div class="log" id="{machine_name}-log"></div>
            </div>""")
        
        html.append('        </div>')
        
        # Add JavaScript with state machine data
        data = {name: {
            'currentState': list(machine['states'].keys())[0],
            'states': machine['states'],
            'transitions': machine['transitions']
        } for name, machine in state_machines.items()}
        
        html.append(self.template_end % json.dumps(data))
        
        return '\n'.join(html)

def main():
    generator = ViewerGenerator()
    html = generator.generate_html('state_machines.json')
    
    with open('state_machine_viewer_generated.html', 'w') as f:
        f.write(html)
    
    print("Generated state_machine_viewer_generated.html")

if __name__ == "__main__":
    main()
