document.addEventListener('DOMContentLoaded', function() {
    const tempValueEl = document.getElementById('temp-value');
    const airHumidityValueEl = document.getElementById('air-humidity-value');
    const soilHumidityValueEl = document.getElementById('soil-humidity-value');
    const vpdValueEl = document.getElementById('vpd-value');

    const lightStatusEl = document.getElementById('light-status');
    const lightOnTimeEl = document.getElementById('light-on-time');
    const lightOffTimeEl = document.getElementById('light-off-time');
    const humidifierStatusEl = document.getElementById('humidifier-status');
    const currentTargetAirHumidityEl = document.getElementById('current-target-air-humidity');

    const targetsForm = document.getElementById('targets-form');
    const targetAirHumidityInput = document.getElementById('target-air-humidity');
    const targetLightOnInput = document.getElementById('target-light-on');
    const targetLightOffInput = document.getElementById('target-light-off');
    const formMessageEl = document.getElementById('form-message');

    function updateSensorUI(data) {
        tempValueEl.textContent = data.temperature !== null ? data.temperature.toFixed(1) : 'ERR';
        airHumidityValueEl.textContent = data.airHumidity !== null ? data.airHumidity.toFixed(1) : 'ERR';
        soilHumidityValueEl.textContent = data.soilHumidity !== null ? data.soilHumidity.toFixed(1) : 'ERR';
        vpdValueEl.textContent = data.vpd !== null ? data.vpd.toFixed(2) : 'ERR';
    }

    function updateStatusUI(data) {
        lightStatusEl.textContent = data.light.isOn ? 'Ligada' : 'Desligada';
        lightOnTimeEl.textContent = data.light.onTime;
        lightOffTimeEl.textContent = data.light.offTime;

        humidifierStatusEl.textContent = data.humidifier.isOn ? 'Ligado' : 'Desligado';
        currentTargetAirHumidityEl.textContent = data.humidifier.targetAirHumidity.toFixed(1);

        // Pre-fill form with current targets for convenience
        if (targetAirHumidityInput.value === '') { // Only if not already filled by user
             targetAirHumidityInput.value = data.humidifier.targetAirHumidity.toFixed(1);
        }
        if (targetLightOnInput.value === '') {
            targetLightOnInput.value = data.light.onTime;
        }
        if (targetLightOffInput.value === '') {
            targetLightOffInput.value = data.light.offTime;
        }
    }

    async function fetchInitialData() {
        try {
            const sensorsResponse = await fetch('/api/sensors');
            if (sensorsResponse.ok) {
                const sensorsData = await sensorsResponse.json();
                updateSensorUI(sensorsData);
            } else {
                console.error('Error fetching sensor data:', sensorsResponse.status);
            }

            const statusResponse = await fetch('/api/status');
            if (statusResponse.ok) {
                const statusData = await statusResponse.json();
                updateStatusUI(statusData);
            } else {
                console.error('Error fetching status data:', statusResponse.status);
            }
        } catch (error) {
            console.error('Failed to fetch initial data:', error);
        }
    }

    // Fetch initial data on load
    fetchInitialData();

    // Server-Sent Events for real-time updates
    if (!!window.EventSource) {
        const source = new EventSource('/events');

        source.addEventListener('sensor_update', function(event) {
            console.log("Sensor update event received");
            const data = JSON.parse(event.data);
            updateSensorUI(data);
        });

        source.addEventListener('status_update', function(event) {
            console.log("Status update event received");
            const data = JSON.parse(event.data);
            updateStatusUI(data);
        });

        source.onerror = function(err) {
            console.error('EventSource failed:', err);
            // Optionally, try to reconnect or notify user
        };
    } else {
        console.warn('EventSource not supported by this browser. No real-time updates.');
        // Fallback to polling if needed, but SSE is preferred
        // setInterval(fetchInitialData, 5000); // Example: Poll every 5 seconds
    }

    // Handle form submission for targets
    targetsForm.addEventListener('submit', async function(event) {
        event.preventDefault();
        formMessageEl.textContent = '';
        formMessageEl.className = 'mt-3';


        const payload = {
            targetAirHumidity: parseFloat(targetAirHumidityInput.value),
            lightOnTime: targetLightOnInput.value,
            lightOffTime: targetLightOffInput.value
        };

        try {
            const response = await fetch('/api/targets', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(payload)
            });

            const result = await response.json();

            if (response.ok && result.success) {
                formMessageEl.textContent = result.message || 'Alvos atualizados com sucesso!';
                formMessageEl.classList.add('alert', 'alert-success');
                // Optionally, refresh status data or wait for SSE update
                // fetchInitialData(); // Or rely on SSE to update the "current target" display
            } else {
                formMessageEl.textContent = result.message || 'Erro ao atualizar alvos.';
                formMessageEl.classList.add('alert', 'alert-danger');
            }
        } catch (error) {
            console.error('Error submitting targets:', error);
            formMessageEl.textContent = 'Erro de comunicação ao enviar alvos.';
            formMessageEl.classList.add('alert', 'alert-danger');
        }
         // Clear message after some time
        setTimeout(() => {
            formMessageEl.textContent = '';
            formMessageEl.className = 'mt-3';
        }, 5000);
    });
});