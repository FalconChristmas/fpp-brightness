

<script type='text/javascript'>
var pixelModels = [];
var excludedModels = [];

function sliderAdjusted() {
    var slider = document.getElementById("brightnessSlider");
    var output = document.getElementById("currentSliderValue");
    var val = slider.value;
    output.innerHTML = val + "%";

    var URL = "/api/plugin-apis/Brightness/" + val;
    $.get(URL);
}

function adjustBrightness(delta) {
    var slider = document.getElementById("brightnessSlider");
    var newValue = parseInt(slider.value) + delta;
    if (newValue < 0) newValue = 0;
    if (newValue > 200) newValue = 200;
    slider.value = newValue;
    sliderAdjusted();
}

function onLoadGetBrightness() {
    var URL = "/api/plugin-apis/Brightness";
    $.get(URL, function (data, status){
          var slider = document.getElementById("brightnessSlider");
          var output = document.getElementById("currentSliderValue");
          slider.value = data;
          output.innerHTML = data + "%";
    });
}

function loadPixelModels(callback) {
    $.get("/api/models", function(data, status) {
        var modelsChanged = false;

        // Check if models have changed
        if (pixelModels.length > 0) {
            modelsChanged = JSON.stringify(pixelModels) !== JSON.stringify(data);
        }

        pixelModels = data;
        updateModelSelect();

        // If models changed and we have exclusions, update them
        if (modelsChanged && excludedModels.length > 0) {
            refreshExclusionsWithNewModels();
        }

        if (callback) callback();
    });
}

function updateModelSelect() {
    var select = document.getElementById("modelSelect");
    select.innerHTML = '<option value="">-- Select a model to exclude --</option>';

    pixelModels.forEach(function(model) {
        var option = document.createElement("option");
        option.value = model.Name;
        option.text = model.Name + " (Ch: " + model.StartChannel + "-" + (model.StartChannel + model.ChannelCount - 1) + ")";
        select.appendChild(option);
    });
}

function addModelExclusion() {
    var select = document.getElementById("modelSelect");
    var modelName = select.value;
    if (!modelName) return;

    var model = pixelModels.find(m => m.Name === modelName);
    if (!model) return;

    var nodeStart = document.getElementById("nodeStart").value;
    var nodeEnd = document.getElementById("nodeEnd").value;

    var totalNodes = model.ChannelCount / model.ChannelCountPerNode;

    var exclusion = {
        modelName: modelName,
        model: model,
        nodeStart: nodeStart || 1,
        nodeEnd: nodeEnd || totalNodes
    };

    excludedModels.push(exclusion);
    updateExclusionList();
    updateExcludeRanges();

    // Reset form
    select.value = "";
    document.getElementById("nodeStart").value = "";
    document.getElementById("nodeEnd").value = "";
}

function removeExclusion(index) {
    excludedModels.splice(index, 1);
    updateExclusionList();
    updateExcludeRanges();
}

function updateExclusionList() {
    var list = document.getElementById("exclusionList");
    list.innerHTML = "";

    if (excludedModels.length === 0) {
        list.className = "exclusion-list-empty";
        list.innerHTML = "No models excluded";
        return;
    }

    list.className = "";

    excludedModels.forEach(function(exc, index) {
        var item = document.createElement("div");
        item.className = "exclusion-item";

        var nodeCount = exc.model.ChannelCount / exc.model.ChannelCountPerNode;
        var text = exc.modelName;

        if (exc.nodeStart != 1 || exc.nodeEnd != nodeCount) {
            text += " (Nodes " + exc.nodeStart + "-" + exc.nodeEnd + ")";
        }

        var startCh = exc.model.StartChannel + ((exc.nodeStart - 1) * exc.model.ChannelCountPerNode);
        var endCh = exc.model.StartChannel + (exc.nodeEnd * exc.model.ChannelCountPerNode) - 1;
        text += " → Ch " + startCh + "-" + endCh;

        item.innerHTML = '<span>' + text + '</span> <button class="btn btn-sm btn-danger" onclick="removeExclusion(' + index + ')">Remove</button>';
        list.appendChild(item);
    });
}

function updateExcludeRanges() {
    var ranges = [];

    excludedModels.forEach(function(exc) {
        var startCh = exc.model.StartChannel + ((exc.nodeStart - 1) * exc.model.ChannelCountPerNode);
        var endCh = exc.model.StartChannel + (exc.nodeEnd * exc.model.ChannelCountPerNode) - 1;
        ranges.push(startCh + "-" + endCh);
    });

    var rangeText = ranges.join(",");
    document.getElementById("BrightnessExcludeRanges").value = rangeText;

    // Save to settings
    if (rangeText) {
        SetSetting("BrightnessExcludeRanges", rangeText, 0, 0, "fpp-brightness");
    } else {
        SetSetting("BrightnessExcludeRanges", "", 0, 0, "fpp-brightness");
    }
}

function loadExistingExclusions() {
    var rangeText = document.getElementById("BrightnessExcludeRanges").value;
    if (!rangeText || rangeText.trim() === "") {
        return;
    }

    // Clear existing exclusions first
    excludedModels = [];

    // Parse the ranges
    var rangeStrings = rangeText.split(",");
    rangeStrings.forEach(function(rangeStr) {
        rangeStr = rangeStr.trim();
        if (!rangeStr) return;

        var parts = rangeStr.split("-");
        if (parts.length !== 2) return;

        var startCh = parseInt(parts[0]);
        var endCh = parseInt(parts[1]);

        // Find which model(s) this range belongs to
        var matchedModel = null;
        var nodeStart = 1;
        var nodeEnd = 1;

        for (var i = 0; i < pixelModels.length; i++) {
            var model = pixelModels[i];
            var modelStart = model.StartChannel;
            var modelEnd = model.StartChannel + model.ChannelCount - 1;

            // Check if this range falls within this model's channel range
            if (startCh >= modelStart && endCh <= modelEnd) {
                matchedModel = model;

                // Calculate which nodes are excluded
                var channelsPerNode = model.ChannelCountPerNode;
                nodeStart = Math.floor((startCh - modelStart) / channelsPerNode) + 1;
                nodeEnd = Math.floor((endCh - modelStart) / channelsPerNode) + 1;

                break;
            }
        }

        if (matchedModel) {
            var exclusion = {
                modelName: matchedModel.Name,
                model: matchedModel,
                nodeStart: nodeStart,
                nodeEnd: nodeEnd
            };
            excludedModels.push(exclusion);
        }
    });

    updateExclusionList();
}

function refreshExclusionsWithNewModels() {
    var updatedExclusions = [];
    var hasChanges = false;

    excludedModels.forEach(function(exc) {
        // Find the updated model
        var updatedModel = pixelModels.find(m => m.Name === exc.modelName);

        if (updatedModel) {
            // Check if model properties changed
            if (updatedModel.StartChannel !== exc.model.StartChannel ||
                updatedModel.ChannelCount !== exc.model.ChannelCount ||
                updatedModel.ChannelCountPerNode !== exc.model.ChannelCountPerNode) {
                hasChanges = true;
            }

            // Update the exclusion with new model data
            updatedExclusions.push({
                modelName: exc.modelName,
                model: updatedModel,
                nodeStart: exc.nodeStart,
                nodeEnd: exc.nodeEnd
            });
        } else {
            // Model no longer exists
            hasChanges = true;
            console.log("Model '" + exc.modelName + "' no longer exists, removing from exclusions");
        }
    });

    if (hasChanges) {
        excludedModels = updatedExclusions;
        updateExclusionList();
        updateExcludeRanges();

        // Show notification
        $.jGrowl("Model configuration changed - brightness exclusions updated", {theme: 'info'});
    }
}

function startModelPolling() {
    // Poll for model changes every 30 seconds
    setInterval(function() {
        loadPixelModels();
    }, 30000);
}

$(document).ready(function() {
    onLoadGetBrightness();

    // Load models, then load existing exclusions once models are ready
    loadPixelModels(function() {
        loadExistingExclusions();
    });

    // Start polling for model changes
    startModelPolling();
});
</script>
<style>
.brightness-control {
  display: flex;
  align-items: center;
  gap: 10px;
  margin: 15px 0;
}

.brightness-btn {
  min-width: 40px;
  height: 40px;
  font-size: 18px;
  font-weight: bold;
}

.brightness-value {
  min-width: 60px;
  text-align: center;
  font-weight: bold;
  font-size: 18px;
}

.slider {
  -webkit-appearance: none;
  appearance: none;
  flex: 1;
  height: 15px;
  background: #d3d3d3;
  outline: none;
  border-radius: 8px;
  transition: opacity .2s;
}

.slider:hover {
  opacity: 1;
}

.slider::-webkit-slider-thumb {
  -webkit-appearance: none;
  appearance: none;
  width: 25px;
  height: 25px;
  background: #4CAF50;
  cursor: pointer;
  border-radius: 50%;
  box-shadow: 0 2px 5px rgba(0,0,0,0.3);
}

.slider::-moz-range-thumb {
  width: 25px;
  height: 25px;
  background: #4CAF50;
  cursor: pointer;
  border-radius: 50%;
  box-shadow: 0 2px 5px rgba(0,0,0,0.3);
}

.model-selector {
  border: 1px solid #ddd;
  border-radius: 5px;
  padding: 15px;
  margin: 15px 0;
  background: #f9f9f9;
}

.model-selector h4 {
  margin-top: 0;
  color: #333;
}

.model-input-group {
  display: flex;
  gap: 10px;
  align-items: flex-end;
  flex-wrap: wrap;
}

.model-input-group > div {
  flex: 1;
  min-width: 150px;
}

.model-input-group label {
  display: block;
  margin-bottom: 5px;
  font-weight: 500;
}

.model-input-group input,
.model-input-group select {
  width: 100%;
  padding: 8px;
  border: 1px solid #ccc;
  border-radius: 4px;
}

.exclusion-item {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 10px;
  margin: 5px 0;
  background: #fff;
  border: 1px solid #ddd;
  border-radius: 4px;
}

.exclusion-item span {
  flex: 1;
}

.exclusion-list-empty {
  color: #999;
  font-style: italic;
  padding: 10px;
  text-align: center;
}

.help-text {
  font-size: 0.9em;
  color: #666;
  margin-top: 5px;
}
</style>


<div id="global" class="settings">
<h2>Brightness Control</h2>
<div class="container-fluid settingsTable settingsGroupTable">

    <!-- Brightness Slider with +/- Buttons -->
    <div class="row">
        <div class="col-md-12">
            <h4>Master Brightness</h4>
            <div class="brightness-control">
                <button class="btn btn-primary brightness-btn" onclick="adjustBrightness(-10)" title="Decrease by 10%">-</button>
                <button class="btn btn-primary brightness-btn" onclick="adjustBrightness(-5)" title="Decrease by 5%">-5</button>
                <input type="range" min="0" max="200" value="100" class="slider" id="brightnessSlider" oninput="sliderAdjusted()">
                <button class="btn btn-primary brightness-btn" onclick="adjustBrightness(5)" title="Increase by 5%">+5</button>
                <button class="btn btn-primary brightness-btn" onclick="adjustBrightness(10)" title="Increase by 10%">+</button>
                <div id="currentSliderValue" class="brightness-value">100%</div>
            </div>
            <div class="help-text">Adjust the overall brightness for all channels (0-200%)</div>
        </div>
    </div>

    <hr>

    <!-- Model Exclusion Selector -->
    <div class="row">
        <div class="col-md-12">
            <div class="model-selector">
                <h4>Exclude Models from Brightness Control</h4>
                <div class="help-text" style="margin-bottom: 15px;">
                    Select models that should remain at 100% brightness regardless of the master brightness setting.
                    Optionally specify node ranges to exclude only specific nodes within a model.
                </div>

                <div class="model-input-group">
                    <div style="flex: 2;">
                        <label>Model:</label>
                        <select id="modelSelect" class="form-control">
                            <option value="">-- Select a model --</option>
                        </select>
                    </div>
                    <div>
                        <label>Start Node: <small>(optional)</small></label>
                        <input type="number" id="nodeStart" class="form-control" min="1" placeholder="1">
                    </div>
                    <div>
                        <label>End Node: <small>(optional)</small></label>
                        <input type="number" id="nodeEnd" class="form-control" min="1" placeholder="All">
                    </div>
                    <div>
                        <label>&nbsp;</label>
                        <button class="btn btn-success" onclick="addModelExclusion()">Add Exclusion</button>
                    </div>
                </div>

                <div style="margin-top: 15px;">
                    <h5>Current Exclusions:</h5>
                    <div id="exclusionList" class="exclusion-list-empty">No models excluded</div>
                </div>
            </div>
        </div>
    </div>

    <hr>

    <!-- Advanced: Manual Range Entry (Hidden by Default) -->
    <div class="row">
        <div class="col-md-12">
            <h5 style="cursor: pointer;" onclick="$('#advancedRanges').toggle();">
                ▸ Advanced: Manual Channel Ranges
                <small class="text-muted">(click to expand)</small>
            </h5>
            <div id="advancedRanges" style="display: none; margin-top: 10px;">
                <div class="help-text" style="margin-bottom: 10px;">
                    Manually specify channel ranges to exclude. Format: startCh-endCh,startCh-endCh
                    <br>Example: 5605-5607,10888-10890
                </div>
                <div class="col-auto">
                    <label>Exclude Ranges:</label>
                    <? PrintSettingTextSaved("BrightnessExcludeRanges", 1, 0, 128, 64, "fpp-brightness"); ?>
                </div>
            </div>
        </div>
    </div>

    <hr>

    <!-- API Information -->
    <div class="row">
        <div class="col-md-12">
            <h5 style="cursor: pointer;" onclick="$('#apiInfo').toggle();">
                ▸ API & Integration Information
                <small class="text-muted">(click to expand)</small>
            </h5>
            <div id="apiInfo" style="display: none; margin-top: 10px;">
                <div class="help-text">
                    <strong>REST API:</strong> GET/POST to <code>http://{hostname}/api/plugin-apis/Brightness/{value}</code>
                    <br>Example: <code>http://<script>document.write(window.location.hostname)</script>/api/plugin-apis/Brightness/75</code> sets brightness to 75%
                    <br><br>
                    <strong>MQTT:</strong> Topic: <code>/Brightness</code>, Payload: brightness value (0-200)
                    <br><br>
                    <strong>MultiSync:</strong> If FPP is in Player mode with MultiSync enabled, brightness changes will be
                    synchronized to all remote FPP instances that have this plugin installed.
                </div>
            </div>
        </div>
    </div>

</div>
</div>
