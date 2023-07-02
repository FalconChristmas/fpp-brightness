

<script type='text/javascript'>
function sliderAdjusted() {
    var slider = document.getElementById("brightnessSlider");
    var output = document.getElementById("currentSliderValue");
    var val = slider.value;
    output.innerHTML = "Current brightness: " + val; // Display the default slider value
    
    var URL = "/api/plugin-apis/Brightness/" + val;
    $.get(URL);
}
function onLoadGetBrightness() {
    var URL = "/api/plugin-apis/Brightness";
    $.get(URL, function (data, status){
          var slider = document.getElementById("brightnessSlider");
          var output = document.getElementById("currentSliderValue");
          slider.value = data;
          output.innerHTML = "Current brightness: " + data;
    });
}
$(document).ready(function() {onLoadGetBrightness()});
</script>
<style>
.slidecontainer {
  width: 100%; /* Width of the outside container */
}

/* The slider itself */
.slider {
  -webkit-appearance: none;  /* Override default CSS styles */
  appearance: none;
  width: 80%; /* Full-width */
  height: 25px; /* Specified height */
  background: #d3d3d3; /* Grey background */
  outline: none; /* Remove outline */
  opacity: 0.7; /* Set transparency (for mouse-over effects on hover) */
  -webkit-transition: .2s; /* 0.2 seconds transition on hover */
  transition: opacity .2s;
}

/* Mouse-over effects */
.slider:hover {
  opacity: 1; /* Fully shown on mouse-over */
}

/* The slider handle (use -webkit- (Chrome, Opera, Safari, Edge) and -moz- (Firefox) to override default look) */
.slider::-webkit-slider-thumb {
  -webkit-appearance: none; /* Override default look */
  appearance: none;
  width: 25px; /* Set a specific slider handle width */
  height: 25px; /* Slider handle height */
  background: #4CAF50; /* Green background */
  cursor: pointer; /* Cursor on hover */
}

.slider::-moz-range-thumb {
  width: 25px; /* Set a specific slider handle width */
  height: 25px; /* Slider handle height */
  background: #4CAF50; /* Green background */
  cursor: pointer; /* Cursor on hover */
}
</style>


<div id="global" class="settings">
<h2>Brightness Plugin</h2>
<div class="container-fluid settingsTable settingsGroupTable">
<div class="row"><div id="currentSliderValue" class="col-md">Current brightness: 100</div></div>
<div class="row"><div class="col-md"><input type="range" min="0" max="200" value="100" class="slider" id="brightnessSlider" oninput="sliderAdjusted()" ></div></div>
<div class="row"><div class="col-auto">Exclude Ranges: </div><div class="col-auto"><? PrintSettingTextSaved("BrightnessExcludeRanges", 1, 0, 128, 64, "fpp-brightness"); ?></div></div>
<div class="row"><div class="col-md">The Brightness plugin also provides a REST api and MQTT api for controlling the brightness.</div></div>
<div class="row"><div class="col-md">For REST, use a URL like http://{ip}/api/plugin-apis/Brightness/100 to set the brightness to 100.</div></div>
<div class="row"><div class="col-md">For MQTT, the sub-topic is "/Brightness" and the payload would be the brightness.</div></div>
<div class="row"><div class="col-md">If FPP is running in Player mode with Multisync enabled, changes to the FPP device's brightness will also be sent to all the remotes that also have the Brightness plugin installed.</div></div>
</div>
</div>
