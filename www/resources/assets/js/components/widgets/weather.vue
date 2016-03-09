<template>
<div class="widget-17 card panel no-border m-a-0 widget-loader-circle">
        <div class="panel-heading">
            <div class="panel-title">
                <i class="pg-map"></i> Detroit, USA
                <span class="caret"></span>
            </div>
            <div class="panel-controls">
                <ul>
                    <li class="">
                        <div class="dropdown">
                            <a data-target="#" href="#" data-toggle="dropdown" aria-haspopup="true"
                               role="button" aria-expanded="false">
                                <i class="portlet-icon portlet-icon-settings"></i>
                            </a>
                            <ul class="dropdown-menu pull-xs-right" role="menu">
                                <li><a href="#">AAPL</a>
                                </li>
                                <li><a href="#">YHOO</a>
                                </li>
                                <li><a href="#">GOOG</a>
                                </li>
                            </ul>
                        </div>
                    </li>
                    <li>
                        <a data-toggle="refresh" class="portlet-refresh text-black" href="#"><i
                                class="portlet-icon portlet-icon-refresh"></i></a>
                    </li>
                </ul>
            </div>
        </div>
        <div class="panel-body">
            <div class="p-l-5">
                <div class="row">
                    <div class="col-md-12 col-xlg-6">
                        <div class="row m-t-20">
                            <div class="col-md-5">
                                <h4 class="m-a-0">Monday</h4>

                                <p class="small text-muted">9th August 2014</p>
                            </div>
                            <div class="col-md-7">
                                <div class="pull-xs-left">
                                    <p class="small text-muted m-a-0">Currently</p>
                                    <h4 class="text-danger bold m-a-0">32°
                                        <span class="small">/ 30F</span>
                                    </h4>
                                </div>
                                <div class="pull-xs-right">
                                    <canvas height="64" width="64" class="clear-day"></canvas>
                                </div>
                            </div>
                        </div>
                        <h5>Feels like
                            <span class="semi-bold">rainy</span>
                        </h5>

                        <p>Weather information</p>

                        <div class="widget-17-weather">
                            <div class="row">
                                <div class="col-xs-6 p-r-10">
                                    <div class="row">
                                        <div class="col-md-12">
                                            <p class="pull-xs-left">Wind</p>

                                            <p class="pull-xs-right bold">11km/h</p>
                                        </div>
                                    </div>
                                    <div class="row">
                                        <div class="col-md-12">
                                            <p class="pull-xs-left">Sunrise</p>

                                            <p class="pull-xs-right bold">05:20</p>
                                        </div>
                                    </div>
                                    <div class="row">
                                        <div class="col-md-12">
                                            <p class="pull-xs-left">Humidity</p>

                                            <p class="pull-xs-right bold">20%</p>
                                        </div>
                                    </div>
                                    <div class="row">
                                        <div class="col-md-12">
                                            <p class="pull-xs-left">Precipitation</p>

                                            <p class="pull-xs-right bold">60%</p>
                                        </div>
                                    </div>
                                </div>
                                <div class="col-xs-6 p-l-10">
                                    <div class="row">
                                        <div class="col-md-12">
                                            <p class="pull-xs-left">Sunset</p>

                                            <p class="pull-xs-right bold">21:05</p>
                                        </div>
                                    </div>
                                    <div class="row">
                                        <div class="col-md-12">
                                            <p class="pull-xs-left">Visibility</p>

                                            <p class="pull-xs-right bold">21km</p>
                                        </div>
                                    </div>
                                </div>
                            </div>
                        </div>
                        <div class="row m-t-10 timeslot">
                            <div class="col-xs-2 p-t-10 text-center">
                                <p class="small">13:30</p>
                                <canvas height="25" width="25" class="partly-cloudy-day"></canvas>
                                <p class="text-danger bold">30°C</p>
                            </div>
                            <div class="col-xs-2 p-t-10 text-center">
                                <p class="small">14:00</p>
                                <canvas height="25" width="25" class="cloudy"></canvas>
                                <p class="text-danger bold">30°C</p>
                            </div>
                            <div class="col-xs-2 p-t-10 text-center">
                                <p class="small">14:30</p>
                                <canvas height="25" width="25" class="rain"></canvas>
                                <p class="text-danger bold">30°C</p>
                            </div>
                            <div class="col-xs-2 p-t-10 text-center">
                                <p class="small">15:00</p>
                                <canvas height="25" width="25" class="sleet"></canvas>
                                <p class="text-danger bold">30°C</p>
                            </div>
                            <div class="col-xs-2 p-t-10 text-center">
                                <p class="small">15:30</p>
                                <canvas height="25" width="25" class="snow"></canvas>
                                <p class="text-danger bold">30°C</p>
                            </div>
                            <div class="col-xs-2 p-t-10 text-center">
                                <p class="small">16:00</p>
                                <canvas height="25" width="25" class="wind"></canvas>
                                <p class="text-danger bold">30°C</p>
                            </div>
                        </div>
                    </div>
                    <div class="col-xl-6 hidden-xl-down">
                        <div class="row">
                            <div class="forecast-day col-md-6 text-center m-t-10 ">
                                <div class="bg-master-lighter p-b-10 p-t-10">
                                    <h4 class="p-t-10 m-a-0">Tuesday</h4>

                                    <p class="small text-muted m-b-20">11th Augest 2014</p>
                                    <canvas class="rain" width="64" height="64"></canvas>
                                    <h5 class="text-danger">32°</h5>

                                    <p>Feels like
                                        <span class="bold">sunny</span>
                                    </p>

                                    <p class="small">Wind
                                        <span class="bold p-l-20">11km/h</span>
                                    </p>

                                    <div class="m-t-20 block">
                                        <div class="padding-10">
                                            <div class="row">
                                                <div class="col-md-6 text-center">
                                                    <p class="small">Noon</p>
                                                    <canvas class="sleet" width="25" height="25"></canvas>
                                                    <p class="text-danger bold">30°C</p>
                                                </div>
                                                <div class="col-md-6 text-center">
                                                    <p class="small">Night</p>
                                                    <canvas class="wind" width="25" height="25"></canvas>
                                                    <p class="text-danger bold">30°C</p>
                                                </div>
                                            </div>
                                        </div>
                                    </div>
                                </div>
                            </div>
                            <div class="col-md-6 text-center m-t-10 ">
                                <div class="bg-master-lighter p-b-10 p-t-10">
                                    <h4 class="p-t-10 m-a-0">Wednesday</h4>

                                    <p class="small text-muted m-b-20">11th Augest 2014</p>
                                    <canvas class="rain" width="64" height="64"></canvas>
                                    <h5 class="text-danger">32°</h5>

                                    <p>Feels like
                                        <span class="bold">sunny</span>
                                    </p>

                                    <p class="small">Wind
                                        <span class="bold p-l-20">11km/h</span>
                                    </p>

                                    <div class="m-t-20 block">
                                        <div class="padding-10">
                                            <div class="row">
                                                <div class="col-md-6 text-center">
                                                    <p class="small">Noon</p>
                                                    <canvas class="sleet" width="25" height="25"></canvas>
                                                    <p class="text-danger bold">30°C</p>
                                                </div>
                                                <div class="col-md-6 text-center">
                                                    <p class="small">Night</p>
                                                    <canvas class="wind" width="25" height="25"></canvas>
                                                    <p class="text-danger bold">30°C</p>
                                                </div>
                                            </div>
                                        </div>
                                    </div>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    </div>
</template>
<script>
export default {
    components: {},
    props: [],
    data() {
        return {};
    },
    ready() {},
    methods: {},
    events: {}

}
</script>

<style lang="sass">
    @import "resources/assets/sass/_vars.scss";
    @import "resources/assets/sass/_mixins.scss";

</style>